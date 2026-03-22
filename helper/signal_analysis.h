/*
signal_analysis.h
*/

#ifndef SIGNAL_ANALYSIS_H
#define SIGNAL_ANALYSIS_H

#define RECORDING_LENGTH   40000
#define FRAME_LENGTH       256
#define HOP_SIZE           128
#define SAMPLING_RATE      8000
#define FEATURES_0         6

#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

#define NUM_MEL_FILTERS    26
#define NUM_MFCC           8

#define EPSILON 1e-7f

int sign_int(int n);
float zero_crossing_rate(int frame[FRAME_LENGTH]);
float spectral_centroid(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
float spectral_bandwidth(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
float dominant_frequency(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
float low_band_power_ratio(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft);
float high_band_power_ratio(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft);

void compute_mel_filterbank(float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS], float f_low, float f_high);
void compute_mfcc(const float fft_magnitude[NO_FREQ_BINS],
                  const float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS],
                  float mfcc_out[NUM_MFCC]);

#endif