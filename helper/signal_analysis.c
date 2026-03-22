/*
signal_analysis.c

This file contains the functions we use to extract relevant data from the signal
and subsequently construct the feature vector. There are five core features common to
each level of implementation:

Noise/Tone/Speech Classifier (Level 0)

1. Zero-Crossing Rate
2. Spectral Centroid
3. Spectral Bandwidth
4. Dominant Frequency
5. Low-Band Power Ratio
6. High-Band Power Ratio

Speaker Identifier (Level 1)
1. Zero-Crossing Rate
2. Spectral Centroid
3. Low-Band Power Ratio
4. High-Band Power Ratio
5. MFCC 1
6. MFCC 2
7. MFCC 3
8. MFCC 4
9. MFCC 5
10. MFCC 6
11. MFCC 7
12. MFCC 8


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

// Zero-Crossing Rate
float zero_crossing_rate(int frame[FRAME_LENGTH]){
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

static float hz_to_mel(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

static float mel_to_hz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

void compute_mel_filterbank(float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS],
                             float f_low, float f_high) {
    float mel_low  = hz_to_mel(f_low);
    float mel_high = hz_to_mel(f_high);

    // M+2 equally spaced points in mel domain, convert back to bin indices
    float mel_points[NUM_MEL_FILTERS + 2];
    int   bin_points[NUM_MEL_FILTERS + 2];
    for (int m = 0; m < NUM_MEL_FILTERS + 2; m++) {
        mel_points[m] = mel_low + (float)m * (mel_high - mel_low) / (NUM_MEL_FILTERS + 1);
        float hz = mel_to_hz(mel_points[m]);
        bin_points[m] = (int)(hz / BIN_SPACING + 0.5f);
        if (bin_points[m] >= NO_FREQ_BINS) bin_points[m] = NO_FREQ_BINS - 1;
    }

    for (int m = 0; m < NUM_MEL_FILTERS; m++) {
        int left   = bin_points[m];
        int centre = bin_points[m + 1];
        int right  = bin_points[m + 2];
        for (int k = 0; k < NO_FREQ_BINS; k++) {
            if      (k <= left || k >= right) filterbank[m][k] = 0.0f;
            else if (k < centre)              filterbank[m][k] = (float)(k - left)  / (float)(centre - left);
            else                              filterbank[m][k] = (float)(right - k) / (float)(right - centre);
        }
    }
}

void compute_mfcc(const float fft_magnitude[NO_FREQ_BINS],
                  const float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS],
                  float mfcc_out[NUM_MFCC]) {
    float log_energies[NUM_MEL_FILTERS];
    for (int m = 0; m < NUM_MEL_FILTERS; m++) {
        float energy = 0.0f;
        for (int k = 0; k < NO_FREQ_BINS; k++)
            energy += filterbank[m][k] * fft_magnitude[k] * fft_magnitude[k];
        log_energies[m] = logf(energy + EPSILON);
    }

    // type-II DCT, skip c0, keep c1..c8
    for (int i = 0; i < NUM_MFCC; i++) {
        int ci = i + 1;
        float sum = 0.0f;
        for (int m = 0; m < NUM_MEL_FILTERS; m++)
            sum += log_energies[m] * cosf(3.14159265f * ci / NUM_MEL_FILTERS * (m + 0.5f));
        mfcc_out[i] = sum;
    }
}