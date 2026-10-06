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

    // --- bench additions (v1.7) ---
    // Rebuild segment geometry and filters at runtime. seg/hop in samples, cutoffs in Hz, decay = running-max decay per hop.
    void configure(int segment_length, int hop, float fc_iq, float fc_pow, float fc_pre, float decay);
    void set_norm(bool on) { norm_ = on; }          // running-max normalisation on/off (off = raw power, Otsu alone handles level)
    bool get_norm() const { return norm_; }
    // Frequency tracking: the phase rotation between successive segment phasors z_k*conj(z_k-1) gives the tone offset from 900 Hz
    // (indicator, always computed); with track on, the phasors are de-rotated by the running estimate (closed loop).
    void set_track(bool on) { track_ = on; if (!on) { f_corr_ = 0.0f; rot_phase_ = 0.0f; } }
    bool get_track() const { return track_; }
    void set_track_gain(float k) { track_gain_ = k; }
    float foff_hz() const { return foff_hz_; }          // smoothed estimate of (tone - 900 Hz), signed
    float foff_conf() const { return foff_conf_; }      // coherence of the last estimate (0..1)
    uint32_t foff_updates() const { return foff_updates_; }
    float f_corr() const { return f_corr_; }
    void reset_state(void);                       // clear filters, ring, normalisation and queues
    int get_segment_length() const { return segment_length_; }
    int get_hop() const { return hop_; }
    float get_fc_iq() const { return fc_iq_; }
    float get_fc_pow() const { return fc_pow_; }
    float get_fc_pre() const { return fc_pre_; }
    float get_decay() const { return decay_; }
    float peak_max() const { return peak_max_; }  // max / min of the raw input samples since reset_peak()
    float peak_min() const { return peak_min_; }
    void reset_peak() { peak_max_ = 0.0f; peak_min_ = 0.0f; }

private:
    void process_segment(void);
    void build(void);                             // (re)allocate buffers and compute all filter coefficients

    // Constants (implicit, not constructor parameters)
    static constexpr float32_t f_sampling_ = 43200.0f;
    static constexpr float32_t f_carrier_ = 900.0f;
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
    float32_t lp_seg_sos_[5];      // I/Q low-pass, computed at runtime for segment rate
    float32_t lp_pow_sos_[5];      // power low-pass
    float32_t pre_sos_[10];        // 2 stages x 5 coefficients (runtime Butterworth 4th)
    static float32_t bessel_sos_[10];

    float fc_iq_, fc_pow_, fc_pre_, decay_;
    float peak_max_, peak_min_;

    // Running normalization
    float32_t running_max_power_;
    bool norm_ = true;

    // Frequency tracking state
    bool track_ = false;
    float track_gain_ = 0.7f;
    float f_corr_ = 0.0f;           // Hz, correction applied by the closed loop
    float rot_phase_ = 0.0f;        // rad
    float zp_re_ = 0.0f, zp_im_ = 0.0f;   // previous (de-rotated) phasor
    float zhi_ = 0.0f;              // slow peak tracker of |z|^2 (gate)
    float S_re_ = 0.0f, S_im_ = 0.0f, S_abs_ = 0.0f;
    int S_n_ = 0, upd_count_ = 0;
    float foff_hz_ = 0.0f, foff_conf_ = 0.0f;
    uint32_t foff_updates_ = 0;

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
