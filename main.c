/* 
FPGA SIGNAL CLASSIFIER
March 2026
*/

//Helper Files
#include "helper/data_processing.h"
#include "helper/signal_analysis.h"
#include "helper/vga.h"
#include "fft_helper/kiss_fftr.h"

//Libraries
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

// Global constants
#define AUDIO_BASE			0xFF203040
#define KEY_BASE            0xFF200050
#define LED_BASE            0xFF200000
#define VGA_BASE            0xFF203020
#define CHARACTER_BASE      0xFF203030

#define RECORDING_LENGTH   40000      // total samples
#define FRAME_LENGTH       256        // samples per frame
#define HOP_SIZE           128        // overlap step
#define SAMPLING_RATE      8000       // Hz
#define FEATURES_0         12         // 12 features in the level 0 feature vector

#define STANDARD_GRAPH_HEIGHT   110
#define STANDARD_GRAPH_WIDTH    130

// Derived constants
#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)

// For real-valued signals: bins 0..N/2 inclusive
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)

// Frequency resolution (Hz per bin)
#define BIN_SPACING        ((double)SAMPLING_RATE / FRAME_LENGTH)

#define RECORD_KEY          0b0001
#define PLAYBACK_KEY        0b0010
#define CLEAR_KEY           0b1111

typedef struct { // Define structure for the audio core registers
	volatile unsigned int control;
	volatile unsigned char rarc;
	volatile unsigned char ralc;
	volatile unsigned char wsrc;
	volatile unsigned char wslc;
	volatile unsigned int ldata;
	volatile unsigned int rdata;
} audio;

audio* audio_ptr = (audio*) AUDIO_BASE; // Instantiate audio pointer
int recording[RECORDING_LENGTH] = {0};

volatile int* key_ptr = (int*) KEY_BASE;

volatile int* led_ptr = (int*) LED_BASE;

volatile int pixel_buffer_start; // Global for drawing target address
short int Buffer1[240][512]; // Buffer 1 memory allocation
short int Buffer2[240][512]; // Buffer 2 memory allocation
volatile int * pixel_ctrl_ptr = (int *)VGA_BASE; // VGA controller address

volatile char* character_buffer_start;
volatile int * character_ctrl_ptr = (int *)CHARACTER_BASE;

// The 2D array which stores the frame-discretized recording
// Each frame is represented by 200 consecutive samples, and any two adjacent
// frames share 100 mutual samples; the last 100 of the first frame and the
// first 100 of the second frame are common.

// A recording of 40k samples consists of 400 frames.
// Each frame is sent to the neural network for processing.
int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH];

// A nested array of FFT's for each frame, where each FFT is 129 samples long
// The reason we only take 129 samples is because the second half of the FFT is redundant
// as the FFT is even for real-valued signals.
double fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS];
double average_fft[NO_FREQ_BINS]; // the pointwise average of every frame. This is what we plot

// Bins that map each sample index (0, 1, 2, 3, ...) in an FFT to its associated linear frequency
double frequency_bins[NO_FREQ_BINS];

int main(void){
    *led_ptr = 0;
    *(key_ptr+3) = CLEAR_KEY;

    character_buffer_start = *character_ctrl_ptr;

    //cons: constant size, color, font.
    //pros: twice higher res
    //note: keep text coordinates alligned with multiples of 8 since each char is 8 x 8.
    //      you can write a maximum of 80x60 characters at one time

    *(pixel_ctrl_ptr + 1) = (int) &Buffer1; // Point back buffer to Buffer1
    waitForVsync(); // Apply buffer settings
    pixel_buffer_start = *pixel_ctrl_ptr; // Sync pointer to front buffer
    clearScreen(); // Clear first buffer
    *(pixel_ctrl_ptr + 1) = (int) &Buffer2; // Point back buffer to Buffer2
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // Sync pointer to back buffer
    clearScreen(); // Clear second buffer

    clearScreen();
    point bode_plot_top_left = {160, 120};
    drawGraphBoundingBox(bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH);
    drawGraphPartitions(5, 5, bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0x39E7);

    point time_domain_plot_top_left = {15, 120};
    drawGraphBoundingBox(time_domain_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH);

    compute_frequency_bins(frequency_bins);

    // Polling the key to get a sample when there is a key edge and record the last 400 samples in a c array
    while (1){
        int edge_reg = *(key_ptr+3);
        if ((edge_reg & RECORD_KEY) == RECORD_KEY) {
            *led_ptr = 1;
            captureRecording();

            kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_LENGTH, 0, NULL, NULL); // configure KissFFT
            unzip_recording_into_frames(frame_array, recording);

            for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
                compute_fft_magnitude(frame_array[frame_idx], fft_array[frame_idx], cfg);
                FeatureVector0* fv = create_feature_vector0(frame_array, fft_array, frequency_bins);
            }

            free(cfg); // free the dynamic memory used by KissFFT
            compute_average_fft(fft_array, avg_fft);
        }

        else if ((edge_reg & PLAYBACK_KEY) == PLAYBACK_KEY) {
            *led_ptr = 2;
            playbackRecording();
        }

        *led_ptr = 0;
        *(key_ptr+3) = CLEAR_KEY;
        
        waitForVsync(); // Wait for screen refresh
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // Switch pointer to new back buffer
    }
}

void captureRecording(){
    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->rarc > 0 && audio_ptr->ralc > 0){
            recording[i] = audio_ptr->ldata;
        }
        else i--;
    }
}

void playbackRecording(){
    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->wsrc > 0 && audio_ptr->wslc > 0){
            audio_ptr->ldata = recording[i];
            audio_ptr->rdata = recording[i];
        }
        else i--;
    }
}
