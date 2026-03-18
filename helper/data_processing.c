/*
data_processing.c

This file contains the functions that will be used to transform raw frame data in samples into real-numbered values
amenable to the neural network for further analysis.
*/

#define RECORDING_LENGTH   40000      // total samples
#define FRAME_LENGTH       256        // samples per frame
#define HOP_SIZE           128        // overlap step
#define SAMPLING_RATE      8000       // Hz
#define FEATURES_0         12         // 12 features in the level 0 feature vector

// Derived constants
#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)

// For real-valued signals: bins 0..N/2 inclusive
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)

// Frequency resolution (Hz per bin)
#define BIN_SPACING        ((double)SAMPLING_RATE / FRAME_LENGTH)

#include <stdlib.h>
#include <math.h>
#include "../kiss_fftr.h"
#include "signal_analysis.h"
#include "data_processing.c"

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH],
                                 const int recording[RECORDING_LENGTH]) {
    // frame 0: samples 0-255
    // frame 1: samples 128-383, etc.
    for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
        int starting_index = HOP_SIZE * frame_idx;

        for (int sample_idx = 0; sample_idx < FRAME_LENGTH; sample_idx++) {
            frame_array[frame_idx][sample_idx] = recording[starting_index + sample_idx];
        }
    }
}

void compute_frequency_bins(double frequency_bins[NO_FREQ_BINS]) {
    for (int k = 0; k < NO_FREQ_BINS; k++) {
        frequency_bins[k] = ((double)k * SAMPLING_RATE) / FRAME_LENGTH;
    }
}

void compute_fft_magnitude(const int frame[FRAME_LENGTH],
                           double fft_frame[NO_FREQ_BINS],
                           kiss_fft_cfg cfg) {

    kiss_fft_scalar fft_input[FRAME_LENGTH];
    kiss_fft_cpx fft_output[NO_FREQ_BINS];

    // Copy input
    for (int i = 0; i < FRAME_LENGTH; i++) {
        fft_input[i] = (kiss_fft_scalar) frame[i];
    }

    // Run FFT
    kiss_fftr(cfg, fft_input, fft_output);

    // Convert to magnitude
    for (int k = 0; k < NO_FREQ_BINS; k++) {
        float real = fft_output[k].r;
        float imag = fft_output[k].i;
        fft_frame[k] = (double) sqrt(real * real + imag * imag);
    }
}

FeatureVector0* create_feature_vector0(int frame[FRAME_LENGTH], double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    FeatureVector0* feature_vector;
    feature_vector -> logEnergy = log_energy(frame);
    feature_vector -> zeroCrossingRate = zero_crossing_rate(frame);
    feature_vector -> spectralCentroid = spectral_centroid(frame_fft, frequency_bins);
    feature_vector -> spectralFlatness = spectral_flatness(frame_fft);
    feature_vector -> spectralBandwidth = spectral_bandwidth(frame_fft, frequency_bins);
    feature_vector -> peakAmplitude = peak_amplitude(frame);
    feature_vector -> crestFactor = crest_factor(frame);
    feature_vector -> dominantFrequency = dominant_frequency(frame_fft, frequency_bins);
    feature_vector -> spectralRolloff = spectral_rolloff(frame_fft, frequency_bins);
    feature_vector -> spectralEntropy = spectral_entropy(frame_fft);
    feature_vector -> lowBandPowerRatio = low_band_power_ratio(frame_fft, frequency_bins);
    feature_vector -> highBandPowerRatio = high_band_power_ratio(frame_fft, frequency_bins);
    return feature_vector;
}

void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]){
    out[0]  = fv->logEnergy;
    out[1]  = fv->zeroCrossingRate;
    out[2]  = fv->spectralCentroid;
    out[3]  = fv->spectralFlatness;
    out[4]  = fv->spectralNandwidth;

    out[5]  = fv->peakAmplitude;
    out[6]  = fv->crestFactor;
    out[7]  = fv->dominantFrequency;
    out[8]  = fv->spectralRolloff;
    out[9]  = fv->spectralEntropy;

    out[10] = fv->lowBandPowerRatio;
    out[11] = fv->highBandPowerRatio;
}