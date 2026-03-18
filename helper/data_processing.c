/*
data_processing.c

This file contains the functions that will be used to transform raw frame data in samples into real-numbered values
amenable to the neural network for further analysis.
*/

#define RECORDING_LENGTH   40000      // total samples
#define FRAME_LENGTH       200        // samples per frame
#define HOP_SIZE           100        // overlap step
#define SAMPLING_RATE      8000       // Hz

// Derived constants
#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)

// For real-valued signals: bins 0..N/2 inclusive
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)

// Frequency resolution (Hz per bin)
#define BIN_SPACING        ((double)SAMPLING_RATE / FRAME_LENGTH)

void unzip_recording_into_frames(int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH],
                                 int recording[RECORDING_LENGTH]) {
    // frame 0: samples 0-199
    // frame 1: samples 100-299
    // frame 2: samples 200-399, etc.
    for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
        int starting_index = HOP_SIZE * frame_idx;

        for (int sample_idx = 0; sample_idx < FRAME_LENGTH; sample_idx++) {
            frame_array[frame_idx][sample_idx] = recording[starting_index + sample_idx];
        }
    }
}

