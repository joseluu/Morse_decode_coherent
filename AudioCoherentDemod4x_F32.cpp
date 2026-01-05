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
int AudioCoherentDemod4x_F32::decimation_factor = 12;


// Coefficients pré-filtre anti-aliasing (Butterworth 4ème ordre à 2*f_carrier)
float32_t AudioCoherentDemod4x_F32::pre_sos[12] = {
    0.000213139f, 0.000426277f, 0.000213139f, 1.559054301f, -0.614051782f,
    1.000000000f, 2.000000000f, 1.000000000f, 1.757753609f, -0.819760443f,
};

// Coefficients filtre Bessel 4ème ordre à f_carrier avant decimation
float32_t AudioCoherentDemod4x_F32::bessel_sos[12] = {
    0.001030600f, 0.002061200f, 0.001030600f, 1.354176305f, -0.464867481f,
    1.000000000f, 2.000000000f, 1.000000000f, 1.434800897f, -0.583770298f,
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

bool AudioCoherentDemod4x_F32::is_frequency_offset_available() const {
    return !power_queue.empty();
}
float AudioCoherentDemod4x_F32::getFrequencyOffsetHz() { 
    float32_t frequency_offset_hz = frequency_offset_queue.front();   // Get the oldest value
    frequency_offset_queue.pop_front();
    return frequency_offset_hz;
}
bool AudioCoherentDemod4x_F32::is_power_value_available() const {
    return !power_queue.empty();
}
float32_t AudioCoherentDemod4x_F32::get_power_value(void){
    float32_t power_value = power_queue.front();   // Get the oldest value
    power_queue.pop_front();
    return power_value;
};

float32_t AudioCoherentDemod4x_F32::get_f_sampling(void) const
{
    return f_sampling;
}

float32_t AudioCoherentDemod4x_F32::get_lowpass_cutoff(void) const
{
    return lowpass_cutoff;
}

void AudioCoherentDemod4x_F32::update(void)
{
    static unsigned int blkCount;

    audio_block_f32_t *in_block = receiveReadOnly_f32();
    if (!in_block) return;

// Allouer 6 blocs de sortie
        // out_blocks[0] power (lp filtered)
        // out_blocks[1] prefilter (after)
        // out_blocks[2] besselfilter (after)
        // out_blocks[3] I subsampled by decimation_factor
        // out_blocks[4] Q subsampled by decimation_factor
        // out_blocks[5] instant phase (subsampled by decimation factor)
    audio_block_f32_t *out_blocks[6] = {nullptr};
    for (int ch = 0; ch < 6; ch++) {
        out_blocks[ch] = allocate_f32();
        if (!out_blocks[ch]) {
            // En cas d'échec, libérer les précédents et sortir
            for (int k = 0; k < ch; k++) release(out_blocks[k]);
            release(in_block);
            return;
        }
    }

    int idx = 0;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float32_t sample = in_block->data[i];

        // 1. Anti-aliasing pre-filter @ f_carrier*2
        arm_biquad_cascade_df1_f32(&pre_filter, &sample, &current_pre, 1);
        sample = current_pre;
        out_blocks[1]->data[idx] = current_pre;

        // todo: decimate to f_carrier *2 and insert search fft

        // Lowpass filtering @ f_carrier
        arm_biquad_cascade_df1_f32(&bessel_filter, &sample, &current_bessel, 1);
        sample = current_bessel;
        out_blocks[2]->data[idx] = current_bessel;

        // 2. Decimation and phase tracking
        sample_counter++;
        if (sample_counter >= decimation_factor) {
            sample_counter = 0;

            switch (phase_counter) {
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

            phase_counter = (phase_counter + 1) & 3;

            // After phase 3 -> full I/Q pair ready
            if (phase_counter == 0) {
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

// Mise à jour des valeurs répétées
                current_phase = instantaneous_phase;
                current_I = I_val;
                current_Q = Q_val;

                // Instantaneous power
                float32_t power = I_val * I_val + Q_val * Q_val;
                arm_biquad_cascade_df1_f32(&lp_filter, &power, &power, 1);

                // Simple running max normalization
                if (power > running_max_power) running_max_power = power;
                if (running_max_power > 1e-12f) {
                    power /= running_max_power;
                }

                current_power = power;
                power_queue.push_back(power);
                
            }
        } // after decimation
        // Répétition des valeurs démodulées sur les sorties debug
        out_blocks[0]->data[idx] = current_power;     // power_filtered
        out_blocks[3]->data[idx] = current_I;         // I répété
        out_blocks[4]->data[idx] = current_Q;         // Q répété
        out_blocks[5]->data[idx] = current_phase;     // phase répétée

        idx++;
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
            frequency_offset_queue.push_back(frequency_offset_hz);
        } else {
            frequency_offset_queue.push_back(999.0f);  // signal error
        }
    }


    //Serial.printf("received block: %6d frequency_offset_hz: %f\n", blkCount++, frequency_offset_hz);


    //Serial.printf("received block: %6d last_power_value: %f\n", blkCount++, running_max_power);

// Transmission des 6 blocs
    for (int ch = 0; ch < 6; ch++) {
        out_blocks[ch]->length = AUDIO_BLOCK_SAMPLES;
        transmit(out_blocks[ch], ch);
        release(out_blocks[ch]);
    }
    release(in_block);
}
