/*
signal_analysis.h
*/

#ifndef SIGNAL_ANALYSIS_H
#define SIGNAL_ANALYSIS_H

#define RECORDING_LENGTH   40000
#define FRAME_LENGTH       256
#define HOP_SIZE           128
#define SAMPLING_RATE      8000
#define FEATURES_0         12

#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

#define EPSILON 1e-7f

int sign_int(int n);

float arithmetic_mean(float frame[NO_FREQ_BINS]);
float geometric_mean(float frame[NO_FREQ_BINS]);

double log_energy(int frame[FRAME_LENGTH]);
double zero_crossing_rate(int frame[FRAME_LENGTH]);

float spectral_centroid(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
float spectral_flatness(float frame_fft[NO_FREQ_BINS]);
float spectral_bandwidth(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);

double peak_amplitude(int frame[FRAME_LENGTH]);
double crest_factor(int frame[FRAME_LENGTH]);

float dominant_frequency(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS]);
float spectral_rolloff(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft);
float spectral_entropy(float frame_fft[NO_FREQ_BINS], float sum_fft);
float low_band_power_ratio(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft);
float high_band_power_ratio(float frame_fft[NO_FREQ_BINS], float frequency_bins[NO_FREQ_BINS], float sum_fft);

#endif