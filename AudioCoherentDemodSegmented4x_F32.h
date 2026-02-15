// AudioCoherentDemodSegmented4x_F32.h
// Segmented coherent demodulator using Hann-windowed overlap analysis
// Combines real-time AudioStream_F32 architecture with windowed I/Q extraction
// from Python coherent_demodulate_segmented algorithm
// Author: Generated for Teensy + OpenAudio F32 library
// Date: February 2026

#ifndef _audio_coherent_demod_segmented4x_f32_h_
#define _audio_coherent_demod_segmented4x_f32_h_

#include "AudioCoherentDemod4x_F32.h"  // shared defines (POWER, I_SAMPLES, etc.), StateChanged

class AudioCoherentDemodSegmented4x_F32 : public AudioStream_F32
{
public:
    // Constructor: segment_length in samples, overlap_factor 0..1 (default 0.5)
    AudioCoherentDemodSegmented4x_F32(int segment_length, float overlap_factor = 0.5f);
    ~AudioCoherentDemodSegmented4x_F32();

    virtual void update(void);

    // Public API (matches AudioCoherentDemod4x_F32 for drop-in compatibility)
    float32_t get_last_power(void);
    float32_t get_last_detection(void);
    float32_t get_threshold();
    void set_threshold(float32_t new_threshold);
    bool has_power_value_available() const;
    float32_t get_power_value(void);
    bool has_state_change_available();
    StateChanged get_state_change();
    float32_t get_f_sampling(void) const;
    float32_t get_lowpass_cutoff(void) const;

private:
    void process_segment(void);

    // Constants (implicit, not constructor parameters)
    static constexpr float32_t f_sampling_ = 43200.0f;
    static constexpr float32_t f_carrier_ = 900.0f;
    static constexpr float32_t lowpass_cutoff_ = 3.0f;
    static constexpr int lo_period_ = 48;  // f_sampling / f_carrier

    // Segment parameters (from constructor)
    int segment_length_;
    int hop_;

    // Dynamically allocated buffers
    float32_t* input_ring_;
    float32_t* hann_window_;

    // Static LO lookup tables (shared across instances)
    static float32_t cos_table_[48];
    static float32_t sin_table_[48];
    static bool lo_tables_initialized_;

    // Ring buffer state
    int ring_write_idx_;
    int ring_fill_count_;

    // Timing
    uint32_t global_sample_counter_;
    int hop_counter_;

    // Detection state
    float32_t detection_threshold_;
    bool above_threshold_;

    // Per-sample filters (same coefficients as AudioCoherentDemod4x_F32)
    arm_biquad_casd_df1_inst_f32 pre_filter_;
    float32_t pre_state_[8];   // 2 stages x 4 states
    arm_biquad_casd_df1_inst_f32 bessel_filter_;
    float32_t bessel_state_[8];

    // Per-segment LP filters (3 independent instances, shared coefficients)
    arm_biquad_casd_df1_inst_f32 lp_filter_I_;
    float32_t lp_state_I_[4];   // 1 stage x 4 states
    arm_biquad_casd_df1_inst_f32 lp_filter_Q_;
    float32_t lp_state_Q_[4];
    arm_biquad_casd_df1_inst_f32 lp_filter_power_;
    float32_t lp_state_power_[4];

    // Coefficient arrays (must persist — CMSIS stores pointers)
    float32_t lp_seg_sos_[5];      // computed at runtime for segment rate
    static float32_t pre_sos_[10]; // 2 stages x 5 coefficients
    static float32_t bessel_sos_[10];

    // Running normalization
    float32_t running_max_power_;

    // Current values for output block repetition
    float32_t current_pre_;
    float32_t current_bessel_;
    float32_t current_I_;
    float32_t current_Q_;
    float32_t current_power_;
    float32_t current_raw_power_;
    float32_t current_phase_;
    float32_t current_detection_;

    // Queues (same pattern as existing class)
    std::deque<float32_t> power_queue_;
    std::deque<StateChanged> state_changes_;

    audio_block_f32_t *inputQueueArray[1];
};

#endif
