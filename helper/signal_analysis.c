/*
signal_analysis.c

This file contains the functions we use to extract relevant data from the signal
and subsequently construct the feature vector. There are five core features common to
each level of implementation:

1. Log Energy
2. Zero-Crossing Rate (ZCR)
3. Spectral Centroid
4. Spectral Flatness
5. Spectral Bandwidth

(Note: All function headers prefixed by "spectral" are analyzed using the FFT of a frame)
The following seven features are unique to our level 0 implementation:

6. Peak Amplitude
7. Crest Factor
8. Dominant Frequency
9. Spectral Rolloff
10. Spectral Entropy
11. Low-Band Power Ratio
12. High-Band Power Ratio
*/

#define FRAME_LENGTH       256
#define NO_FREQ_BINS       129
#define SAMPLING_RATE      8000
#define EPSILON            1e-7f

#include "signal_analysis.h"
#include <math.h>
#include <stdlib.h>

// Sign function for integers
int sign_int(int n){
    if (n > 0) return 1;
    else if (n < 0) return -1;
    else return 0;
}

// Arithmetic Mean
float arithmetic_mean(float frame[NO_FREQ_BINS]){
    float sum = 0.0f;
    for (int i = 0; i < NO_FREQ_BINS; i++){
        sum += frame[i];
    }
    return sum / NO_FREQ_BINS;
}

// Geometric Mean
float geometric_mean(float frame[NO_FREQ_BINS]){
    float log_sum = 0.0f;
    for (int i = 0; i < NO_FREQ_BINS; i++){
        log_sum += logf(frame[i] + EPSILON);
    }
    return expf(log_sum / NO_FREQ_BINS);
}

// Log Energy
double log_energy(int frame[FRAME_LENGTH]){
    double running_sum = 0.0;
    for (int n = 0; n < FRAME_LENGTH; n++){
        running_sum += (frame[n] * frame[n]);
    }
    return log(running_sum + 1e-12);
}

// Zero-Crossing Rate
double zero_crossing_rate(int frame[FRAME_LENGTH]){
    int running_sum = 0;
    for (int n = 1; n < FRAME_LENGTH; n++){
        running_sum += abs(sign_int(frame[n]) - sign_int(frame[n-1]));
    }
    return (double) running_sum / (2 * FRAME_LENGTH);
}

// Spectral Centroid
float spectral_centroid(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]){
    float sum_top = 0.0f;
    float sum_bottom = 0.0f;
    for (int k = 0; k < NO_FREQ_BINS; k++){
        sum_top    += frequency_bins[k] * frame_fft[k];
        sum_bottom += frame_fft[k];
    }
    return sum_top / (sum_bottom + EPSILON);
}

// Spectral Flatness
float spectral_flatness(float frame_fft[NO_FREQ_BINS]){
    return geometric_mean(frame_fft) / (arithmetic_mean(frame_fft) + EPSILON);
}

// Spectral Bandwidth
float spectral_bandwidth(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]){
    float sum_top = 0.0f;
    float sum_bottom = 0.0f;
    float centroid = spectral_centroid(frame_fft, frequency_bins);
    for (int k = 0; k < NO_FREQ_BINS; k++){
        float inner_term = frequency_bins[k] - centroid;
        sum_top    += (inner_term * inner_term) * frame_fft[k];
        sum_bottom += frame_fft[k];
    }
    return sqrtf(sum_top / (sum_bottom + EPSILON));
}

// Peak Amplitude
double peak_amplitude(int frame[FRAME_LENGTH]){
    int max_value = abs(frame[0]);
    for (int i = 1; i < FRAME_LENGTH; i++){
        if (abs(frame[i]) > max_value){
            max_value = abs(frame[i]);
        }
    }
    return (double) max_value;
}

// Crest Factor
double crest_factor(int frame[FRAME_LENGTH]){
    double sum_of_squares = 0.0;
    for (int i = 0; i < FRAME_LENGTH; i++){
        sum_of_squares += (frame[i] * frame[i]);
    }
    return peak_amplitude(frame) / (sqrt(sum_of_squares / FRAME_LENGTH) + 1e-12);
}

// Dominant Frequency
float dominant_frequency(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]){
    int dominant_k = 0;
    float max_value = frame_fft[0];
    for (int k = 1; k < NO_FREQ_BINS; k++){
        if (frame_fft[k] > max_value){
            dominant_k = k;
            max_value = frame_fft[k];
        }
    }
    return frequency_bins[dominant_k];
}

// Spectral Rolloff
// sum_fft is precomputed in create_feature_vector0 and passed in
float spectral_rolloff(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft){
    float running_sum = 0.0f;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        running_sum += frame_fft[l];
        if (running_sum >= 0.85f * sum_fft){
            return frequency_bins[l];
        }
    }
    return frequency_bins[NO_FREQ_BINS - 1];
}

// Spectral Entropy
// sum_fft is precomputed in create_feature_vector0 and passed in
float spectral_entropy(float frame_fft[NO_FREQ_BINS], float sum_fft){
    float entropy = 0.0f;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        float p_l = frame_fft[l] / (sum_fft + EPSILON);
        entropy += p_l * logf(p_l + EPSILON);
    }
    return -entropy;
}

// Low-Band Power Ratio
// sum_fft is precomputed in create_feature_vector0 and passed in
float low_band_power_ratio(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft){
    float f_L = 1000.0f;
    int stopping_index = 0;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        if (frequency_bins[l] >= f_L){ stopping_index = l; break; }
    }
    float running_sum = 0.0f;
    for (int k = 0; k < stopping_index; k++){
        running_sum += frame_fft[k];
    }
    return running_sum / (sum_fft + EPSILON);
}

// High-Band Power Ratio
// sum_fft is precomputed in create_feature_vector0 and passed in
float high_band_power_ratio(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft){
    float f_H = 2000.0f;
    int starting_index = 0;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        if (frequency_bins[l] >= f_H){ starting_index = l; break; }
    }
    float running_sum = 0.0f;
    for (int k = starting_index; k < NO_FREQ_BINS; k++){
        running_sum += frame_fft[k];
    }
    return running_sum / (sum_fft + EPSILON);
}