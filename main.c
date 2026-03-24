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
#define KEYBOARD_BASE       0xFF200100

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

#define KEY_A               0x1C
#define KEY_B               0x32
// #define KEY_1               0x16
// #define KEY_2               0x1E
// #define KEY_3               0x26
#define KEY_LEFT            0x6B
#define KEY_RIGHT           0x74

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
volatile int* keyboard_ptr = (int*) KEYBOARD_BASE;

volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];
volatile int * pixel_ctrl_ptr = (int *)VGA_BASE;

volatile char* character_buffer_start;
volatile int * character_ctrl_ptr = (int *)CHARACTER_BASE;

/*
DISPLAY GRAPH LEGEND
1-Time Domain
2-Magnitude Spectrum
3-Spectrogram
*/
int DISPLAY_GRAPH = 1;
int PREV_DISPLAY_GRAPH = 1;
int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH];

// Changed from double to float — soft-float emulation of 32-bit ops is
// substantially cheaper than 64-bit on the NiosV rv32im target
float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS];
float average_fft[NO_FREQ_BINS];
float frequency_bins[NO_FREQ_BINS];
float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS];

int max_sample_amplitude = 1;
bool record = false;
bool playback = false;
bool redraw = false;

int captureRecording();

void playbackRecording();
void displayMagnitudeSpectrum();
void displayTime();
void displaySpectrogram();
void displayCorrectGraph();

static inline bool ps2_read(unsigned char *out);

int main(void){
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

    const char* button1 = "Time";
    const char* button2 = "Spectrum";
    const char* button3 =  "Spectrogram";

    // Draw buttons into back buffer
    createGraphButton(button1, (point){25, 80});
    createGraphButton(button2, (point){55, 80});
    createGraphButton(button3, (point){100, 80});

    // Swap, then draw into the other buffer too
    waitForVsync();
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    createGraphButton(button1, (point){25, 80});
    createGraphButton(button2, (point){55, 80});
    createGraphButton(button3, (point){100, 80});

    // Inital graph draw
    clearRegion((point){0, 95}, 320, 145);
    displayCorrectGraph();
    waitForVsync();
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    clearRegion((point){0, 95}, 320, 145);
    displayCorrectGraph();
    waitForVsync();
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);

    static bool ps2_break_pending = false;
    static bool ps2_extend_pending = false;

    // PS/2 Keyboard Polling Loop
    while (1){
        unsigned char byte; // char because its 8bit

        while (ps2_read(&byte)){
            if (byte == 0xF0){
                ps2_break_pending = true;
                continue;
            } else if (byte == 0xE0){
                ps2_extend_pending = true;
                continue;
            }

            bool is_break = ps2_break_pending;
            bool is_extended = ps2_extend_pending;

            ps2_break_pending = false;
            ps2_extend_pending = false;

            if (!is_break){
                if (byte == KEY_A) record = true;
                if (byte == KEY_B) playback = true;

                if (is_extended && byte == KEY_LEFT)  DISPLAY_GRAPH = ((DISPLAY_GRAPH - 1) + 3) % 3;
                if (is_extended && byte == KEY_RIGHT) DISPLAY_GRAPH = (DISPLAY_GRAPH + 1) % 3;
            }
        }

        if (DISPLAY_GRAPH != PREV_DISPLAY_GRAPH){
            if (PREV_DISPLAY_GRAPH == 3){
                point spectrogram_top_left = {25, 100};
                clearSpectrogramLabel(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40);
            }
            clearRegion((point){0, 95}, 320, 145);
            displayCorrectGraph();
            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);

            clearRegion((point){0, 95}, 320, 145);
            displayCorrectGraph();
            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);

            PREV_DISPLAY_GRAPH = DISPLAY_GRAPH;
        }

        if (record){
            record = false;
            *led_ptr = 0x1;
            max_sample_amplitude = captureRecording();

            *led_ptr = 0x20;
            kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_LENGTH, 0, NULL, NULL);
            unzip_recording_into_frames(frame_array, recording);
            
            for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
                compute_fft_magnitude(frame_array[frame_idx], fft_array[frame_idx], cfg);
            }

            free(cfg);
            compute_average_fft(fft_array, average_fft);

            clearRegion((point){0, 95}, 320, 145);
            displayCorrectGraph();
            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);

            clearRegion((point){0, 95}, 320, 145);
            displayCorrectGraph();
            waitForVsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);
            *led_ptr = 0;
        }

        if (playback){
            playback = false;
            *led_ptr = 0x2;
            playbackRecording();
            *led_ptr = 0;
        }

    }
}

int captureRecording(){
    *led_ptr = 1;
    int max_sample_amplitude = 0;
    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->rarc > 0 && audio_ptr->ralc > 0){
            recording[i] = audio_ptr->ldata;
            int abs_val = abs(recording[i]);
            max_sample_amplitude = abs_val > max_sample_amplitude ? abs_val : max_sample_amplitude;
        }
        else i--;
    }
    *led_ptr = 0;
    return max_sample_amplitude;
}

static inline bool ps2_read(unsigned char *out) {
    int val = *keyboard_ptr;
    if (val & 0x8000) { // bit 15 of the status register is the RVALID bit: if this is low,
                        // the FIFO is empty.
        *out = (unsigned char)(val & 0xFF); // masks the bottom 8 bits, which is the data.
                                            // the top 4 bits of the data encodes whether the keyboard detected a release (0xF0)
        return true;
    }
    return false;
}

void playbackRecording(){
    *led_ptr = 2;
    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->wsrc > 0 && audio_ptr->wslc > 0){
            audio_ptr->ldata = recording[i];
            audio_ptr->rdata = recording[i];
        }
        else i--;
    }
    *led_ptr = 0;
}

void displayMagnitudeSpectrum(){
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
    drawGraphBoundingBox(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40);
    drawXAxisLabels(5, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40, 0xFFFF, 5.0, "s");
    drawYAxisLabels(5, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40, 0xFFFF, (double)frequency_bins[NO_FREQ_BINS-1], "Hz");
    drawSpectrogramLabel(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40);
    plotSpectrogram(fft_array, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, 230);
}

void displayCorrectGraph(){
    if (DISPLAY_GRAPH == 0){
        *led_ptr |= 0x4;
        displayTime();
    } else if (DISPLAY_GRAPH == 1){
        displayMagnitudeSpectrum();
        *led_ptr |= 0x8;
    } else if (DISPLAY_GRAPH == 2){
        *led_ptr |= 0x10;
        displaySpectrogram();
    } else {
        // if it doesn't know which graph to display, always display the time domain plot.
        displayTime();
    }

    return;
}