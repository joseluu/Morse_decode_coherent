// AudioCoherentDemod4x_F32.h
// Coherent demodulator for 4x oversampled signal (hardware-style)
// Assumes: AUDIO_SAMPLE_RATE_EXACT is an integer multiple of (4 * f_carrier)
// Author: Generated for Teensy + OpenAudio F32 library
// Date: January 2026

#ifndef _audio_coherent_demod4x_f32_h_
#define _audio_coherent_demod4x_f32_h_

#include <AudioStream_F32.h>
#include <arm_math.h>
#include <deque>

#define PHASE_BUFFER_SIZE 2000  // Fixed size for sliding window

class AudioCoherentDemod4x_F32 : public AudioStream_F32
{
public:
    // Constructor: f_carrier in Hz, lowpass_cutoff in Hz (default 30 Hz)
    AudioCoherentDemod4x_F32();
    
    // AudioStream_F32 required method
    virtual void update(void);

// Getter: returns estimated frequency offset in Hz (positive = received > carrier)
    bool is_frequency_offset_available() const;
    float getFrequencyOffsetHz();
    bool is_power_value_available() const;
    float32_t get_power_value(void);
    float32_t get_f_sampling(void) const;
    float32_t get_lowpass_cutoff(void) const;

private:
// constants setup in the cpp file
    static float32_t f_carrier;
    static float32_t f_sampling;
    static float32_t fs_resampled;
    static float32_t lowpass_cutoff;
    static int   decimation_factor;

    // Runtime state
    int   sample_counter = 0;
    int   phase_counter = 0; // 0,1,2,3 corresponding to 0°, 90°, 180°, 270°

    float32_t I_val = 0.0f;
    float32_t Q_val = 0.0f;
    float32_t running_max_power = 0.0f; // For simple normalization
    float32_t last_power_value = 0.0f; // For simple normalization

// Phase tracking: sliding window of 2000 unwrapped phases
    float32_t phase_buffer[PHASE_BUFFER_SIZE];
    int       phase_buffer_idx = 0;
    int       phase_buffer_count = 0;  // Current number of samples in buffer
    float32_t previous_phase = 0.0f;   // For unwrapping

    // Estimated frequency offset (updated at end of each block)
    float     frequency_offset_hz = 0.0f;

    // Small queues for power and phase
    std::deque<float32_t> power_queue;
    std::deque<float32_t> frequency_offset_queue;  // unwrapped phase

// Current values to repeat on debug outputs
    float32_t current_pre = 0.0f;
    float32_t current_bessel = 0.0f;
    float32_t current_I = 0.0f;
    float32_t current_Q = 0.0f;
    float32_t current_power = 0.0f;
    float32_t current_phase = 0.0f;

    // Anti-aliasing pre-filter (4th order Butterworth at 2*f_carrier)
    arm_biquad_casd_df1_inst_f32 pre_filter;
    float32_t pre_state[8]; // 2 biquad stages × 4 states
    arm_biquad_casd_df1_inst_f32 bessel_filter;
    float32_t bessel_state[8];  // 2 stages × 4 states

    // Lowpass filter for I/Q and final envelope (2nd order Butterworth)
    arm_biquad_casd_df1_inst_f32 lp_filter;
    float32_t lp_state[4];  // 1 stage × 4 states


    // Coefficients - MUST be generated for your exact fs_orig and f_carrier
    // Use Python script (see below) to compute and paste values here
    static float32_t pre_sos[12];  // 2 sections × 6 coefficients (b0,b1,b2,1,a1,a2)
    static float32_t bessel_sos[12];
    static float32_t lp_sos[6];     // 1 section  × 6 coefficients
    
    audio_block_f32_t *inputQueueArray[1];
};

#endif