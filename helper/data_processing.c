/*
data_processing.c

This file contains the functions that will be used to transform raw frame data in samples into real-numbered values
amenable to the neural network for further analysis.
*/

#define RECORDING_LENGTH   40000      // total samples
#define FRAME_LENGTH       256        // samples per frame
#define HOP_SIZE           128        // overlap step
#define SAMPLING_RATE      8000       // Hz

// Derived constants
#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)

// For real-valued signals: bins 0..N/2 inclusive
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)

// Frequency resolution (Hz per bin)
#define BIN_SPACING        ((double)SAMPLING_RATE / FRAME_LENGTH)

#include <stdlib.h>
#include <math.h>
#include "kiss_fftr.h"

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

void compute_fft_magnitude(const int frame[FRAME_LENGTH],
                           double fft_frame[NO_FREQ_BINS]) {

    kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_LENGTH, 0, NULL, NULL);

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

    free(cfg);
}
