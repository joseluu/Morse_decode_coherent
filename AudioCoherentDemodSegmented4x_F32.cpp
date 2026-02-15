// AudioCoherentDemodSegmented4x_F32.cpp
// Implementation of segmented coherent demodulator for Teensy Audio F32
// Uses Hann-windowed overlap analysis for cleaner I/Q extraction

#include "AudioCoherentDemodSegmented4x_F32.h"
#include <math.h>

// --- Static LO lookup tables ---
float32_t AudioCoherentDemodSegmented4x_F32::cos_table_[48];
float32_t AudioCoherentDemodSegmented4x_F32::sin_table_[48];
bool AudioCoherentDemodSegmented4x_F32::lo_tables_initialized_ = false;

// --- Pre-filter coefficients: Butterworth 4th order at 2*f_carrier = 1800 Hz ---
// (same values as AudioCoherentDemod4x_F32)
float32_t AudioCoherentDemodSegmented4x_F32::pre_sos_[10] = {
    0.000213139f, 0.000426277f, 0.000213139f, 1.559054301f, -0.614051782f,
    1.000000000f, 2.000000000f, 1.000000000f, 1.757753609f, -0.819760443f,
};

// --- Bessel filter coefficients: 4th order at f_carrier = 900 Hz ---
// (same values as AudioCoherentDemod4x_F32)
float32_t AudioCoherentDemodSegmented4x_F32::bessel_sos_[10] = {
    0.001030600f, 0.002061200f, 0.001030600f, 1.354176305f, -0.464867481f,
    1.000000000f, 2.000000000f, 1.000000000f, 1.434800897f, -0.583770298f,
};


// --- Constructor ---
AudioCoherentDemodSegmented4x_F32::AudioCoherentDemodSegmented4x_F32(
    int segment_length, float overlap_factor)
    : AudioStream_F32(1, inputQueueArray),
      segment_length_(segment_length),
      hop_((int)(segment_length * (1.0f - overlap_factor))),
      ring_write_idx_(0),
      ring_fill_count_(0),
      global_sample_counter_(0),
      detection_threshold_(0.22f),
      above_threshold_(false),
      running_max_power_(0.0f),
      current_pre_(0.0f),
      current_bessel_(0.0f),
      current_I_(0.0f),
      current_Q_(0.0f),
      current_power_(0.0f),
      current_raw_power_(0.0f),
      current_phase_(0.0f),
      current_detection_(0.0f)
{
    hop_counter_ = hop_;

    // Allocate and zero the ring buffer
    input_ring_ = new float32_t[segment_length_];
    memset(input_ring_, 0, segment_length_ * sizeof(float32_t));

    // Precompute Hann window: w[k] = 0.5 * (1 - cos(2*pi*k/N))
    hann_window_ = new float32_t[segment_length_];
    for (int k = 0; k < segment_length_; k++) {
        hann_window_[k] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * k / segment_length_));
    }

    // Initialize LO lookup tables (once, shared across all instances)
    if (!lo_tables_initialized_) {
        for (int i = 0; i < lo_period_; i++) {
            cos_table_[i] = cosf(2.0f * (float)M_PI * i / (float)lo_period_);
            sin_table_[i] = sinf(2.0f * (float)M_PI * i / (float)lo_period_);
        }
        lo_tables_initialized_ = true;
    }

    // Initialize per-sample filters with same coefficients as existing class
    memset(pre_state_, 0, sizeof(pre_state_));
    memset(bessel_state_, 0, sizeof(bessel_state_));
    arm_biquad_cascade_df1_init_f32(&pre_filter_, 2, pre_sos_, pre_state_);
    arm_biquad_cascade_df1_init_f32(&bessel_filter_, 2, bessel_sos_, bessel_state_);

    // Compute LP biquad coefficients for segment rate
    // Butterworth 2nd order LP via bilinear transform
    // fc = 3 Hz, fs_seg = 43200 / hop
    float32_t fs_seg = f_sampling_ / (float32_t)hop_;
    float32_t K = tanf((float)M_PI * lowpass_cutoff_ / fs_seg);
    float32_t K2 = K * K;
    float32_t sqrt2K = sqrtf(2.0f) * K;
    float32_t norm = 1.0f + sqrt2K + K2;
    lp_seg_sos_[0] = K2 / norm;                     // b0
    lp_seg_sos_[1] = 2.0f * K2 / norm;              // b1
    lp_seg_sos_[2] = K2 / norm;                     // b2
    lp_seg_sos_[3] = 2.0f * (1.0f - K2) / norm;    // a1 (CMSIS sign convention)
    lp_seg_sos_[4] = -(1.0f - sqrt2K + K2) / norm; // a2 (CMSIS sign convention)

    // Initialize 3 independent LP filter instances (shared coefficients)
    memset(lp_state_I_, 0, sizeof(lp_state_I_));
    memset(lp_state_Q_, 0, sizeof(lp_state_Q_));
    memset(lp_state_power_, 0, sizeof(lp_state_power_));
    arm_biquad_cascade_df1_init_f32(&lp_filter_I_, 1, lp_seg_sos_, lp_state_I_);
    arm_biquad_cascade_df1_init_f32(&lp_filter_Q_, 1, lp_seg_sos_, lp_state_Q_);
    arm_biquad_cascade_df1_init_f32(&lp_filter_power_, 1, lp_seg_sos_, lp_state_power_);
}


// --- Destructor ---
AudioCoherentDemodSegmented4x_F32::~AudioCoherentDemodSegmented4x_F32()
{
    delete[] input_ring_;
    delete[] hann_window_;
}


// --- Public API ---

float32_t AudioCoherentDemodSegmented4x_F32::get_last_power(void) {
    return current_power_;
}

float32_t AudioCoherentDemodSegmented4x_F32::get_last_detection(void) {
    return current_detection_;
}

float32_t AudioCoherentDemodSegmented4x_F32::get_threshold() {
    return detection_threshold_;
}

void AudioCoherentDemodSegmented4x_F32::set_threshold(float32_t new_threshold) {
    detection_threshold_ = new_threshold;
}

bool AudioCoherentDemodSegmented4x_F32::has_power_value_available() const {
    return !power_queue_.empty();
}

float32_t AudioCoherentDemodSegmented4x_F32::get_power_value(void) {
    float32_t value = power_queue_.front();
    power_queue_.pop_front();
    return value;
}

bool AudioCoherentDemodSegmented4x_F32::has_state_change_available() {
    return !state_changes_.empty();
}

StateChanged AudioCoherentDemodSegmented4x_F32::get_state_change() {
    StateChanged value = state_changes_.front();
    state_changes_.pop_front();
    return value;
}

float32_t AudioCoherentDemodSegmented4x_F32::get_f_sampling(void) const {
    return f_sampling_;
}

float32_t AudioCoherentDemodSegmented4x_F32::get_lowpass_cutoff(void) const {
    return lowpass_cutoff_;
}


// --- update() — called once per 128-sample audio block ---
void AudioCoherentDemodSegmented4x_F32::update(void)
{
    audio_block_f32_t *in_block = receiveReadOnly_f32();
    if (!in_block) return;

    // Allocate 9 output blocks
    audio_block_f32_t *out_blocks[OUTPUT_BLOCK_COUNT] = {nullptr};
    for (int ch = 0; ch < OUTPUT_BLOCK_COUNT; ch++) {
        out_blocks[ch] = allocate_f32();
        if (!out_blocks[ch]) {
            for (int k = 0; k < ch; k++) release(out_blocks[k]);
            release(in_block);
            return;
        }
    }

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float32_t sample = in_block->data[i];

        // 1. Anti-aliasing pre-filter (Butterworth 4th @ 1800 Hz)
        arm_biquad_cascade_df1_f32(&pre_filter_, &sample, &current_pre_, 1);
        out_blocks[ANTI_ALIASING]->data[i] = current_pre_;

        // 2. Bessel filter (4th @ 900 Hz) — cascaded after pre-filter
        float32_t bessel_in = current_pre_;
        arm_biquad_cascade_df1_f32(&bessel_filter_, &bessel_in, &current_bessel_, 1);
        out_blocks[FCARRIER_BESSEL]->data[i] = current_bessel_;

        // 3. Write pre-filtered sample to ring buffer
        input_ring_[ring_write_idx_] = current_pre_;
        ring_write_idx_ = (ring_write_idx_ + 1) % segment_length_;
        if (ring_fill_count_ < segment_length_) ring_fill_count_++;
        global_sample_counter_++;

        // 4. Check if it's time to process a segment
        hop_counter_--;
        bool tick = false;
        if (hop_counter_ <= 0 && ring_fill_count_ >= segment_length_) {
            process_segment();
            hop_counter_ = hop_;
            tick = true;
        }

        // 5. Fill output blocks with current repeated values
        out_blocks[POWER]->data[i] = current_power_;
        out_blocks[I_SAMPLES]->data[i] = current_I_;
        out_blocks[Q_SAMPLES]->data[i] = current_Q_;
        out_blocks[PHASE_SAMPLES]->data[i] = current_phase_;
        out_blocks[SUBSAMPLE_TICKS]->data[i] = tick ? 1.0f : 0.0f;
        out_blocks[DETECTION_SAMPLES]->data[i] = current_detection_;
        out_blocks[RAW_POWER]->data[i] = current_raw_power_;
    }

    // Transmit and release all blocks
    for (int ch = 0; ch < OUTPUT_BLOCK_COUNT; ch++) {
        out_blocks[ch]->length = AUDIO_BLOCK_SAMPLES;
        transmit(out_blocks[ch], ch);
        release(out_blocks[ch]);
    }
    release(in_block);
}


// --- process_segment() — called every hop_ samples when buffer is full ---
void AudioCoherentDemodSegmented4x_F32::process_segment(void)
{
    float32_t I_sum = 0.0f;
    float32_t Q_sum = 0.0f;

    for (int k = 0; k < segment_length_; k++) {
        // Read from ring buffer: oldest (k=0) to newest (k=segment_length_-1)
        int ring_idx = (ring_write_idx_ + k) % segment_length_;
        // LO phase for this sample's global position
        uint32_t lo_idx = (global_sample_counter_ - segment_length_ + k) % lo_period_;

        float32_t windowed = input_ring_[ring_idx] * hann_window_[k];
        I_sum += windowed * cos_table_[lo_idx];
        Q_sum += windowed * sin_table_[lo_idx];
    }

    // Normalize: factor of 2 for Hann window amplitude correction
    I_sum *= 2.0f / segment_length_;
    Q_sum *= 2.0f / segment_length_;

    // LP filter I and Q independently
    float32_t I_filt, Q_filt;
    arm_biquad_cascade_df1_f32(&lp_filter_I_, &I_sum, &I_filt, 1);
    arm_biquad_cascade_df1_f32(&lp_filter_Q_, &Q_sum, &Q_filt, 1);

    current_I_ = I_filt;
    current_Q_ = Q_filt;
    current_phase_ = atan2f(Q_filt, I_filt);

    // Raw power from filtered I/Q
    float32_t raw_power = I_filt * I_filt + Q_filt * Q_filt;

    // LP filter the power
    float32_t power;
    arm_biquad_cascade_df1_f32(&lp_filter_power_, &raw_power, &power, 1);

    // Running max normalization (same 0.9999 decay as existing class)
    if (power > running_max_power_)
        running_max_power_ = power;
    else
        running_max_power_ *= 0.9999f;

    if (running_max_power_ > 1e-12f) {
        raw_power /= running_max_power_;
        power /= running_max_power_;
    }

    current_raw_power_ = raw_power;
    current_power_ = power;

    // Detection thresholding with state change tracking
    if (power > detection_threshold_) {
        if (!above_threshold_) {
            state_changes_.push_back(StateChanged(true, global_sample_counter_));
            above_threshold_ = true;
        }
    } else {
        if (above_threshold_) {
            state_changes_.push_back(StateChanged(false, global_sample_counter_));
            above_threshold_ = false;
        }
    }
    if (state_changes_.size() > 10) {
        state_changes_.pop_front();
    }

    current_detection_ = above_threshold_ ? 1.0f : 0.0f;

    // Power queue for external consumers
    power_queue_.push_back(power);
    if (power_queue_.size() > 10) {
        power_queue_.pop_front();
    }
}
