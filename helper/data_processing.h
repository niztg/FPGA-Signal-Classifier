/*
data_processing.h
*/

#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H

#define FRAME_LENGTH             256         // locked
#define HOP_SIZE                 128         // locked
#define SAMPLING_RATE            8000
#define FEATURES_0               6
#define FEATURES_1               20          // 4 spectral + 8 mfcc means + 8 mfcc stds

#define MAX_RECORDING_LENGTH     80000       // 10s at 8000 Hz
#define CHUNK_DURATION_SAMPLES   (SAMPLING_RATE / 2)                                    // 0.5s = 4000 samples
#define FRAMES_PER_CHUNK         ((CHUNK_DURATION_SAMPLES - FRAME_LENGTH) / HOP_SIZE + 1) // 30
#define MAX_CHUNKS_PER_RECORDING 20
#define MAX_FRAMES_PER_RECORDING (MAX_CHUNKS_PER_RECORDING * FRAMES_PER_CHUNK)          // 600

#define NO_FREQ_BINS             ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING              ((float)SAMPLING_RATE / FRAME_LENGTH)

#define MAX_DISPLAY_BINS    NO_FREQ_BINS+(7*20)
#define MIN_DISPLAY_BINS    NO_FREQ_BINS-(6*20)

#include "../fft_helper/kiss_fftr.h"
#include "signal_analysis.h"  // for NUM_MFCC
#include <stdbool.h>

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

typedef struct {
    int frame_array[MAX_FRAMES_PER_RECORDING][FRAME_LENGTH];
    float fft_array[MAX_FRAMES_PER_RECORDING][NO_FREQ_BINS];
    short int fill_color;
    short int std_color;
    bool has_been_run;
    int result_buffer[MAX_CHUNKS_PER_RECORDING];
    int recording[MAX_RECORDING_LENGTH];
    float average_fft[MAX_DISPLAY_BINS];
    FeatureVector1 feature_vector_array[MAX_CHUNKS_PER_RECORDING];
    int time_plot_line_heights[STANDARD_GRAPH_WIDTH/2];
    int recording_length;
    int n_chunks;
    int frames_per_recording;
    int samples_per_pixel;
} Channel;

void unzip_recording_into_frames(int frame_array[MAX_FRAMES_PER_RECORDING][FRAME_LENGTH], const int recording[MAX_RECORDING_LENGTH], int n_frames);
void compute_fft_magnitude(const int frame[FRAME_LENGTH], float fft_frame[NO_FREQ_BINS], kiss_fftr_cfg cfg);
void compute_frequency_bins(float frequency_bins[NO_FREQ_BINS]);
void create_feature_vector0(FeatureVector0* fv, int frame[FRAME_LENGTH], float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]);
void compute_average_fft(float fft_magnitude[MAX_FRAMES_PER_RECORDING][NO_FREQ_BINS], float avg_fft[NO_FREQ_BINS]);
float get_max_value(float arr[], int length);

void create_feature_vector1(FeatureVector1* fv,
                             int frame_array[MAX_FRAMES_PER_RECORDING][FRAME_LENGTH],
                             float fft_array[MAX_FRAMES_PER_RECORDING][NO_FREQ_BINS],
                             float frequency_bins[NO_FREQ_BINS],
                             const float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS]);
void flatten_feature_vector1(FeatureVector1* fv, float out[FEATURES_1]);
void create_feature_vector1_chunk(FeatureVector1* fv,
                                   int frame_array[MAX_FRAMES_PER_RECORDING][FRAME_LENGTH],
                                   float fft_array[MAX_FRAMES_PER_RECORDING][NO_FREQ_BINS],
                                   float frequency_bins[NO_FREQ_BINS],
                                   const float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS],
                                   int start, int end);

#endif
