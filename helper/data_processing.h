/*
data_processing.h
*/

#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H

#define RECORDING_LENGTH   40000
#define FRAME_LENGTH       256
#define HOP_SIZE           128
#define SAMPLING_RATE      8000
#define FEATURES_0         12

#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

#include "../fft_helper/kiss_fftr.h"

typedef struct {
    double logEnergy;           // kept as double — computed from integer samples
    double zeroCrossingRate;    // kept as double — computed from integer samples
    float spectralCentroid;
    float spectralFlatness;
    float spectralBandwidth;
    double peakAmplitude;       // kept as double — computed from integer samples
    double crestFactor;         // kept as double — computed from integer samples
    float dominantFrequency;
    float spectralRolloff;
    float spectralEntropy;
    float lowBandPowerRatio;
    float highBandPowerRatio;
} FeatureVector0;

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH], const int recording[RECORDING_LENGTH]);
void compute_fft_magnitude(const int frame[FRAME_LENGTH], float fft_frame[NO_FREQ_BINS], kiss_fftr_cfg cfg);
void compute_frequency_bins(float frequency_bins[NO_FREQ_BINS]);
FeatureVector0* create_feature_vector0(int frame[FRAME_LENGTH], float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]);
void compute_average_fft(float fft_magnitude[FRAMES_PER_RECORDING][NO_FREQ_BINS], float avg_fft[NO_FREQ_BINS]);
double get_max_value(double arr[], int length);

#endif