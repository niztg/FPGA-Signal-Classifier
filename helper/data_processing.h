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

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH], int recording[RECORDING_LENGTH]);
void compute_fft_magnitude(const int frame[FRAME_LENGTH], double fft_frame[NO_FREQ_BINS]);
#endif