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
#define FEATURES_0         6

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
float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS];

int max_sample_amplitude = 1;

bool cur_sw1 = true;
bool prev_sw1;

int captureRecording();
void playbackRecording();
void displayBode();
void displayTime();
void displaySpectrogram();
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
    compute_mel_filterbank(filterbank, 80.0f, 4000.0f);

    // Before the while(1) loop, after compute_mel_filterbank:
    cur_sw1 = (*sw_ptr & SW1_TIMEPLOT) == SW1_TIMEPLOT;
    prev_sw1 = !cur_sw1;   // force first-iteration mismatch → guaranteed initial draw

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
            }

            free(cfg);
            compute_average_fft(fft_array, average_fft);
            displayCorrectGraph();

            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);

            /*
            COLLECT TRAINING DATA
            LEGEND (LEVEL 0)
            TONE: 0
            NOISE: 1
            SPEECH: 2

            PRINT FEATURE VECTOR FOR EACH FRAME, ALONG WITH DESIGNATION OF SIGNAL TYPE
            PRINTF THIS INFORMATION OUT, WILL BE POOLED INTO THE DATA.CSV FILE
            SEND THIS PRINTED DATA TO AN EXTERNAL MACHINE FOR TRAINING.

            ORDER FOR LEVEL0:
            (ZCR, Spectral Centroid, Spectral Bandwidth, Dominant Frequency, LBPR, HBPR, *designation*)
            */
            // SW7 selects collection mode: 0 = level 0, 1 = level 1
            int collect_mode = (*sw_ptr >> 7) & 0b1;  // 0 = level 0 only, 1 = combined

            if (!collect_mode) {
                // level 0 only — original behaviour, SW8-9 selects label
                int label0 = (*sw_ptr >> 8) & 0b11;
                *led_ptr |= 0b100;
                for (int i = 0; i < FRAMES_PER_RECORDING; i++) {
                    if (i % 4 != 0) continue;
                    FeatureVector0 fv;
                    double feature_vector[FEATURES_0];
                    create_feature_vector0(&fv, frame_array[i], fft_array[i], frequency_bins);
                    flatten_feature_vector(&fv, feature_vector);
                    for (int j = 0; j < FEATURES_0; j++) printf("%.4f,", feature_vector[j]);
                    printf("%d\n", label0);
                }
                *led_ptr &= ~0b100;

            } else {
                // combined — speech only, so level 0 label is always 2
                // SW8: 0 = unauthorized, 1 = authorized (level 1 label)
                int label1 = (*sw_ptr >> 8) & 0b1;
                *led_ptr |= 0b100;

                // level 0 per-frame rows, label hardcoded to 2 (speech)
                for (int i = 0; i < FRAMES_PER_RECORDING; i++) {
                    if (i % 4 != 0) continue;
                    FeatureVector0 fv;
                    double feature_vector[FEATURES_0];
                    create_feature_vector0(&fv, frame_array[i], fft_array[i], frequency_bins);
                    flatten_feature_vector(&fv, feature_vector);
                    for (int j = 0; j < FEATURES_0; j++) printf("%.3f,", feature_vector[j]);
                    printf("%d\n", 2);  // always speech
                }

                // level 1 single row
                // level 1: one row per chunk
                for (int chunk = 0; chunk < CHUNKS_PER_RECORDING; chunk++) {
                    int start = chunk * FRAMES_PER_CHUNK;
                    int end   = start + FRAMES_PER_CHUNK;
                    FeatureVector1 fv1;
                    float fv1_flat[FEATURES_1];
                    create_feature_vector1_chunk(&fv1, frame_array, fft_array, frequency_bins, filterbank, start, end);
                    flatten_feature_vector1(&fv1, fv1_flat);
                    for (int j = 0; j < FEATURES_1; j++) printf("%.4f,", fv1_flat[j]);
                    printf("%d\n", label1);
                }

                *led_ptr &= ~0b100;
            }

        }

        else if ((edge_reg & PLAYBACK_KEY) == PLAYBACK_KEY) {
            *led_ptr = 2;
            playbackRecording();
        }

        //when done
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
    drawXAxisLabels(5, bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0xFFFF, (double) frequency_bins[NO_FREQ_BINS-1], x_axis_units);
    plotMagnitudeSpectrum(
        average_fft,
        bode_plot_top_left,
        STANDARD_GRAPH_WIDTH,
        STANDARD_GRAPH_HEIGHT,
        0xFDE0,
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

void displaySpectrogram(){
    point spectrogram_top_left = {25, 100};
    const char* x_axis_units = "s";
    const char* y_axis_units = "kHz";

    drawGraphBoundingBox(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH);
    drawXAxisLabels(5, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0xFFFF, 5.0, x_axis_units);
    drawYAxisLabels(5, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0xFFFF, (double) frequency_bins[NO_FREQ_BINS-1], y_axis_units);
    plotSpectrogram(
        fft_array,
        spectrogram_top_left,
        STANDARD_GRAPH_HEIGHT,
        STANDARD_GRAPH_WIDTH
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
        // displayBode();
        displaySpectrogram();
    }
}
