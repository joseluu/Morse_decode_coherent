// AudioCoherentDemod4x_F32.cpp
// Implementation of 4x coherent demodulator for Teensy Audio F32
// Optimized for integer decimation ratio

#include "AudioCoherentDemod4x_F32.h"

// Placeholder coefficients - REPLACE with real values generated via Python
// Example for fs=44100, f_carrier=735 Hz (2940 Hz resampled, decimation=15)
// f_carrier choisi = 735.000 Hz
// fs_resampled (4*fc) = 2940.0 Hz
// Facteur de décimation = 15 (entier → parfait !) sampling @ 44100Hz

// Parametres du traitement
float32_t AudioCoherentDemod4x_F32::f_carrier = 900.000f;
float32_t AudioCoherentDemod4x_F32::lowpass_cutoff = 3.000f;
float32_t AudioCoherentDemod4x_F32::f_sampling = 43200.000f;
int decimation_factor = 12;


// Coefficients pré-filtre anti-aliasing (Butterworth 4ème ordre à 2*f_carrier)
float32_t AudioCoherentDemod4x_F32::pre_sos[12] = {
    0.000213139f, 0.000426277f, 0.000213139f, 1.559054301f, -0.614051782f,
    1.000000000f, 2.000000000f, 1.000000000f, 1.757753609f, -0.819760443f,
};

// Coefficients filtre Bessel 4ème ordre à f_carrier avant decimation
float32_t AudioCoherentDemod4x_F32::bessel_sos[12] = {
    0.000072333f, 0.000144665f, 0.000072333f, 1.667249718f, -0.696988525f,
    1.000000000f, 2.000000000f, 1.000000000f, 1.731916122f, -0.770832303f,
};

// Coefficients filtre final enveloppe (Butterworth 2ème ordre)
float32_t AudioCoherentDemod4x_F32::lp_sos[6] = {
    0.000108058f, 0.000216116f, 0.000108058f, 1.970382898f, -0.970815131f
};

AudioCoherentDemod4x_F32::AudioCoherentDemod4x_F32()
    : AudioStream_F32(1, inputQueueArray)
{
    // Initialize filters using SOS format
    arm_biquad_cascade_df1_init_f32(&pre_filter, 2, pre_sos, pre_state);
    arm_biquad_cascade_df1_init_f32(&bessel_filter, 2, bessel_sos, bessel_state);
    arm_biquad_cascade_df1_init_f32(&lp_filter, 1, lp_sos, lp_state);
}

float32_t AudioCoherentDemod4x_F32::read(void)
{
    return last_power_value;
}

float32_t AudioCoherentDemod4x_F32::get_f_sampling(void)
{
    return f_sampling;
}

float32_t AudioCoherentDemod4x_F32::get_lowpass_cutoff(void)
{
    return lowpass_cutoff;
}

void AudioCoherentDemod4x_F32::update(void)
{
    static unsigned int blkCount;

    audio_block_f32_t *in_block = receiveReadOnly_f32();
    if (!in_block) return;

    audio_block_f32_t *out_block = allocate_f32();
    if (!out_block) {
        release(in_block);
        return;
    }

    int out_idx = 0;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float32_t sample = in_block->data[i];

        // 1. Anti-aliasing pre-filter @ f_carrier*2
        arm_biquad_cascade_df1_f32(&pre_filter, &sample, &sample, 1);

        // Lowpass filtering todo: move after decimation ?
        arm_biquad_cascade_df1_f32(&bessel_filter, &sample, &sample, 1);

        // 2. Decimation and phase tracking 
        // todo: insert fft on decimated samples
        sample_counter++;
        if (sample_counter >= decimation_factor) {
            sample_counter = 0;

            switch (current_phase) {
                case 0:  // 0°   -> +I
                    I_val = sample;
                    break;
                case 1:  // 90°  -> +Q
                    Q_val = sample;
                    break;
                case 2:  // 180° -> -I
                    I_val = -sample;
                    break;
                case 3:  // 270° -> -Q
                    Q_val = -sample;
                    break;
            }

            current_phase = (current_phase + 1) & 3;

            // After phase 3 -> full I/Q pair ready
            if (current_phase == 0) {


// Instantaneous phase (unwrapped across blocks)
                float32_t instantaneous_phase = atan2f(Q_val, I_val);
                static float32_t previous_phase = 0.0f;
                float32_t delta = instantaneous_phase - previous_phase;

                // Unwrap: bring delta into -π..π range
                if (delta > M_PI) delta -= 2.0f * M_PI;
                if (delta < -M_PI) delta += 2.0f * M_PI;

                previous_phase = instantaneous_phase;  // update for next

                float32_t unwrapped_phase = previous_phase + delta;  // actually same as instantaneous_phase after correction

// Add to sliding buffer
                phase_buffer[phase_buffer_idx] = unwrapped_phase;
                phase_buffer_idx = (phase_buffer_idx + 1) % PHASE_BUFFER_SIZE;
                if (phase_buffer_count < PHASE_BUFFER_SIZE) phase_buffer_count++;

                // Instantaneous power
                float32_t power = I_val * I_val + Q_val * Q_val;
                arm_biquad_cascade_df1_f32(&lp_filter, &power, &power, 1);

                // Simple running max normalization
                if (power > running_max_power) running_max_power = power;
                if (running_max_power > 1e-12f) {
                    power /= running_max_power;
                }
// Zero-pad output if needed
                if (out_idx < AUDIO_BLOCK_SAMPLES) {
                    out_block->data[out_idx++] = power;
                    last_power_value = power;
                }
            }
        }
    }

// === Slope calculation on sliding window ===
    if (phase_buffer_count >= PHASE_BUFFER_SIZE) {
// Least-squares fit: phase = slope * idx + intercept
        float sum_x = 0.0f, sum_y = 0.0f, sum_xx = 0.0f, sum_xy = 0.0f;
        for (int k = 0; k < PHASE_BUFFER_SIZE; k++) {
            int buf_idx = (phase_buffer_idx + k) % PHASE_BUFFER_SIZE;  // Sliding order: oldest to newest
            float x = (float)k;  // Relative index 0 to 1999
            float y = phase_buffer[buf_idx];
            sum_x += x;
            sum_y += y;
            sum_xx += x * x;
            sum_xy += x * y;
        }
        float denom = PHASE_BUFFER_SIZE * sum_xx - sum_x * sum_x;
        if (fabs(denom) > 1e-12f) {
            float slope_rad_per_sample = (PHASE_BUFFER_SIZE * sum_xy - sum_x * sum_y) / denom;
            // Convert to Hz: slope / (2π) * fs_decimated
            frequency_offset_hz = (slope_rad_per_sample / (2.0f * M_PI)) * f_carrier;
        }
    }


    //Serial.printf("received block: %6d frequency_offset_hz: %f\n", blkCount++, frequency_offset_hz);

    // Zero-pad remaining output samples
    while (out_idx < AUDIO_BLOCK_SAMPLES) {
        out_block->data[out_idx++] = 0.0f;
    }

    //Serial.printf("received block: %6d last_power_value: %f\n", blkCount++, running_max_power);

    out_block->length = AUDIO_BLOCK_SAMPLES;
    transmit(out_block);
    release(out_block);
    release(in_block);
}
