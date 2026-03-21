/* 
FPGA SIGNAL CLASSIFIER
March 2026
*/

#include "helper/data_processing.h"
#include "helper/signal_analysis.h"
#include "helper/vga.h"
#include "fft_helper/kiss_fftr.h"

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define AUDIO_BASE          0xFF203040
#define KEY_BASE            0xFF200050
#define LED_BASE            0xFF200000
#define VGA_BASE            0xFF203020
#define CHARACTER_BASE      0xFF203030
#define SWITCH_BASE         0xFF200040

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

#define RECORD_KEY          0b0001
#define PLAYBACK_KEY        0b0010
#define CLEAR_KEY           0b1111

#define SW1_TIMEPLOT        1

typedef struct {
    volatile unsigned int control;
    volatile unsigned char rarc;
    volatile unsigned char ralc;
    volatile unsigned char wsrc;
    volatile unsigned char wslc;
    volatile unsigned int ldata;
    volatile unsigned int rdata;
} audio;

audio* audio_ptr = (audio*) AUDIO_BASE;
int recording[RECORDING_LENGTH] = {0};

volatile int* key_ptr   = (int*) KEY_BASE;
volatile int* led_ptr   = (int*) LED_BASE;
volatile int* sw_ptr    = (int*) SWITCH_BASE;

volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];
volatile int * pixel_ctrl_ptr = (int *)VGA_BASE;

volatile char* character_buffer_start;
volatile int * character_ctrl_ptr = (int *)CHARACTER_BASE;

int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH];

// Changed from double to float — soft-float emulation of 32-bit ops is
// substantially cheaper than 64-bit on the NiosV rv32im target
float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS];
float average_fft[NO_FREQ_BINS];
float frequency_bins[NO_FREQ_BINS];

int max_sample_amplitude;

bool cur_sw1 = true;
bool prev_sw1;

int captureRecording();
void playbackRecording();
void displayBode();
void displayTime();
void displayCorrectGraph();

int main(void){
    *led_ptr = 0;
    *(key_ptr+3) = CLEAR_KEY;

    character_buffer_start = (volatile char*) *character_ctrl_ptr;

    // Initialise double buffer — no explicit clearScreen needed,
    // the pixel buffers are zeroed by the hardware on reset
    *(pixel_ctrl_ptr + 1) = (int) &Buffer1;
    waitForVsync();
    pixel_buffer_start = *pixel_ctrl_ptr;

    *(pixel_ctrl_ptr + 1) = (int) &Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    compute_frequency_bins(frequency_bins);

    while (1){
        prev_sw1 = cur_sw1;
        cur_sw1 = (*sw_ptr & SW1_TIMEPLOT) == SW1_TIMEPLOT;

        if (prev_sw1 != cur_sw1) {
            displayCorrectGraph();
            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        }

        int edge_reg = *(key_ptr+3);

        if ((edge_reg & RECORD_KEY) == RECORD_KEY) {
            *led_ptr = 1;
            max_sample_amplitude = captureRecording();

            *led_ptr = 0b1000000000;
            kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_LENGTH, 0, NULL, NULL);
            unzip_recording_into_frames(frame_array, recording);

            for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
                compute_fft_magnitude(frame_array[frame_idx], fft_array[frame_idx], cfg);

                // Fixed: pass the current frame's row, not the entire 2D array
                FeatureVector0* fv = create_feature_vector0(
                    frame_array[frame_idx],
                    fft_array[frame_idx],
                    frequency_bins
                );
                // Fixed: free the heap allocation returned by create_feature_vector0
                free(fv);
            }

            free(cfg);
            compute_average_fft(fft_array, average_fft);

            displayCorrectGraph();
            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        }
        else if ((edge_reg & PLAYBACK_KEY) == PLAYBACK_KEY) {
            *led_ptr = 2;
            playbackRecording();
        }

        *led_ptr = 0;
        *(key_ptr+3) = CLEAR_KEY;
    }
}

int captureRecording(){
    int max_sample_amplitude = 0;
    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->rarc > 0 && audio_ptr->ralc > 0){
            recording[i] = audio_ptr->ldata;
            int abs_val = abs(recording[i]);
            max_sample_amplitude = abs_val > max_sample_amplitude ? abs_val : max_sample_amplitude;
        }
        else i--;
    }
    return max_sample_amplitude;
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

void displayBode(){
    *led_ptr = 0b01000000000;

    point bode_plot_top_left = {25, 100};

    const char* x_axis_units = "Hz";

    drawGraphBoundingBox(bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH);
    drawGraphGrid(5, 7, bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0x39E7, 3);
    drawXAxisLabels(7, bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0xFFFF, (double) frequency_bins[NO_FREQ_BINS-1], x_axis_units);
    plotMagnitudeSpectrum(
        average_fft,
        bode_plot_top_left,
        STANDARD_GRAPH_WIDTH,
        STANDARD_GRAPH_HEIGHT,
        0xFDE0
    );
}

void displayTime(){
    *led_ptr = 0b0010000000;

    point time_plot_mid_left = {25, 160};

    plotTimeDomain(time_plot_mid_left,
        STANDARD_GRAPH_WIDTH,
        STANDARD_GRAPH_HEIGHT,
        RECORDING_LENGTH,
        max_sample_amplitude
    );
}

void displayCorrectGraph(){
    // Clear only the region the graph occupies rather than the full 320x240 screen.
    // Both plot types fit within this rectangle.
    point graph_region = {15, 90};
    clearRegion(graph_region, 295, 155);

    if (cur_sw1 == SW1_TIMEPLOT){
        displayTime();
    } else {
        displayBode();
    }
}