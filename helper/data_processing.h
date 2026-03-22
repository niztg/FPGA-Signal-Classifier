/*
data_processing.h
*/

#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H

#define RECORDING_LENGTH   40000
#define FRAME_LENGTH       256
#define HOP_SIZE           128
#define SAMPLING_RATE      8000
#define FEATURES_0         6
#define FEATURES_1         20   // 4 spectral + 8 mfcc means + 8 mfcc stds


#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

#define CHUNKS_PER_RECORDING 10
#define FRAMES_PER_CHUNK     (FRAMES_PER_RECORDING / CHUNKS_PER_RECORDING)

#include "../fft_helper/kiss_fftr.h"
#include "signal_analysis.h"  // for NUM_MFCC

typedef struct {
    float zeroCrossingRate;    // kept as double — computed from integer samples
    float spectralCentroid;
    float spectralBandwidth;
    float dominantFrequency;
    float lowBandPowerRatio;
    float highBandPowerRatio;
} FeatureVector0;

typedef struct {
    float zeroCrossingRate;
    float spectralCentroid;
    float lowBandPowerRatio;
    float highBandPowerRatio;
    float mfcc_mean[NUM_MFCC];
    float mfcc_std[NUM_MFCC];
} FeatureVector1;


void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH], const int recording[RECORDING_LENGTH]);
void compute_fft_magnitude(const int frame[FRAME_LENGTH], float fft_frame[NO_FREQ_BINS], kiss_fftr_cfg cfg);
void compute_frequency_bins(float frequency_bins[NO_FREQ_BINS]);
void create_feature_vector0(FeatureVector0* fv, int frame[FRAME_LENGTH], float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]);
void compute_average_fft(float fft_magnitude[FRAMES_PER_RECORDING][NO_FREQ_BINS], float avg_fft[NO_FREQ_BINS]);
float get_max_value(float arr[], int length);

void create_feature_vector1(FeatureVector1* fv,
                             int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH],
                             float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS],
                             float frequency_bins[NO_FREQ_BINS],
                             const float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS]);
void flatten_feature_vector1(FeatureVector1* fv, float out[FEATURES_1]);
void create_feature_vector1_chunk(FeatureVector1* fv,
                                   int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH],
                                   float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS],
                                   float frequency_bins[NO_FREQ_BINS],
                                   const float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS],
                                   int start, int end);

#endif