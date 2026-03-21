/*
data_processing.c

This file contains the functions that will be used to transform raw frame data in samples into real-numbered values
amenable to the neural network for further analysis.
*/

#define RECORDING_LENGTH   40000
#define FRAME_LENGTH       256
#define HOP_SIZE           128
#define SAMPLING_RATE      8000
#define FEATURES_0         6

#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

#include <stdlib.h>
#include <math.h>
#include "../fft_helper/kiss_fftr.h"
#include "signal_analysis.h"
#include "data_processing.h"

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH],
                                 const int recording[RECORDING_LENGTH]) {
    for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
        int starting_index = HOP_SIZE * frame_idx;
        for (int sample_idx = 0; sample_idx < FRAME_LENGTH; sample_idx++) {
            frame_array[frame_idx][sample_idx] = recording[starting_index + sample_idx];
        }
    }
}

void compute_frequency_bins(float frequency_bins[NO_FREQ_BINS]) {
    for (int k = 0; k < NO_FREQ_BINS; k++) {
        frequency_bins[k] = ((float)k * SAMPLING_RATE) / FRAME_LENGTH;
    }
}

void compute_fft_magnitude(const int frame[FRAME_LENGTH],
                           float fft_frame[NO_FREQ_BINS],
                           kiss_fftr_cfg cfg) {
    kiss_fft_scalar fft_input[FRAME_LENGTH];
    kiss_fft_cpx fft_output[NO_FREQ_BINS];

    for (int i = 0; i < FRAME_LENGTH; i++) {
        fft_input[i] = (kiss_fft_scalar) frame[i];
    }

    kiss_fftr(cfg, fft_input, fft_output);

    for (int k = 0; k < NO_FREQ_BINS; k++) {
        float real = fft_output[k].r;
        float imag = fft_output[k].i;
        fft_frame[k] = sqrtf(real * real + imag * imag);
    }
}

void create_feature_vector0(FeatureVector0* fv, int frame[FRAME_LENGTH], float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]){
    float sum_fft = 0.0f;
    for (int k = 0; k < NO_FREQ_BINS; k++) sum_fft += frame_fft[k];

    fv->zeroCrossingRate   = zero_crossing_rate(frame);
    fv->spectralCentroid   = spectral_centroid(frame_fft, frequency_bins);
    fv->spectralBandwidth  = spectral_bandwidth(frame_fft, frequency_bins);
    fv->dominantFrequency  = dominant_frequency(frame_fft, frequency_bins);
    fv->lowBandPowerRatio  = low_band_power_ratio(frame_fft, frequency_bins, sum_fft);
    fv->highBandPowerRatio = high_band_power_ratio(frame_fft, frequency_bins, sum_fft);
}

void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]){
    out[0]  = fv->zeroCrossingRate;
    out[1]  = (double) fv->spectralCentroid;
    out[2]  = (double) fv->spectralBandwidth;
    out[3]  = (double) fv->dominantFrequency;
    out[4] = (double) fv->lowBandPowerRatio;
    out[5] = (double) fv->highBandPowerRatio;
}

void compute_average_fft(
    float fft_magnitude[FRAMES_PER_RECORDING][NO_FREQ_BINS],
    float avg_fft[NO_FREQ_BINS]
){
    for (int k = 0; k < NO_FREQ_BINS; k++){
        avg_fft[k] = 0.0f;
    }
    for (int frame = 0; frame < FRAMES_PER_RECORDING; frame++){
        for (int k = 0; k < NO_FREQ_BINS; k++){
            avg_fft[k] += fft_magnitude[frame][k];
        }
    }
    for (int k = 0; k < NO_FREQ_BINS; k++){
        avg_fft[k] /= FRAMES_PER_RECORDING;
    }
}

float get_max_value(float arr[], int length) {
    if (length <= 0) return 0.0f;
    float max = arr[0];
    for (int i = 1; i < length; i++) {
        if (arr[i] > max) max = arr[i];
    }
    return max;
}