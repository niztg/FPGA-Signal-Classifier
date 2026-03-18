/*
data_processing.c

This file contains the functions that will be used to transform raw frame data in samples into real-numbered values
amenable to the neural network for further analysis.
*/

#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H

#define RECORDING_LENGTH        40000
#define FRAMES_PER_RECORDING    399
#define FRAME_LENGTH            200
#define NO_FREQ_BINS            100
#define SAMPLING_RATE           8000
#define FEATURES_0              12

typedef struct {
    double log_energy;
    double zero_crossing_rate;
    double spectral_centroid;
    double spectral_flatness;
    double spectral_bandwidth;
    double peak_amplitude;
    double crest_factor;
    double dominant_frequency;
    double spectral_rolloff;
    double spectral_entropy;
    double low_band_power_ratio;
    double high_band_power_ratio;
} FeatureVector0;

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH], int recording[RECORDING_LENGTH]);
void compute_fft_magnitude(const int frame[FRAME_LENGTH], double fft_frame[NO_FREQ_BINS]);
void compute_frequency_bins(double frequency_bins[NO_FREQ_BINS]);
FeatureVector0* create_feature_vector0(int frame[FRAME_LENGTH], double frame_fft[NO_FREQ_BINS]);
void flatten_feature_vector(FeatureVector0* fv, double out[FEATURES_0]);

#endif