/*
data_processing.c

This file contains the functions that will be used to transform raw frame data in samples into real-numbered values
amenable to the neural network for further analysis.
*/

#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H

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

#include "../kiss_fftr.h"

typedef struct {
    double logEnergy;
    double zeroCrossingRate;
    double spectralCentroid;
    double spectralFlatness;
    double spectralBandwidth;
    double peakAmplitude;
    double crestFactor;
    double dominantFrequency;
    double spectralRolloff;
    double spectralEntropy;
    double lowBandPowerRatio;
    double highBandPowerRatio;
} FeatureVector0;

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH], const int recording[RECORDING_LENGTH]);
void compute_fft_magnitude(const int frame[FRAME_LENGTH], double fft_frame[NO_FREQ_BINS], kiss_fft_cfg cfg);
void compute_frequency_bins(double frequency_bins[NO_FREQ_BINS]);
FeatureVector0* create_feature_vector0(int frame[FRAME_LENGTH], double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);
void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]);

#endif