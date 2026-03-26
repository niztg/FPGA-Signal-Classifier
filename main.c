/* 
FPGA SIGNAL CLASSIFIER
March 2026

Code from this commit is MILESTONE #2 READY! 
*/

#include "helper/data_processing.h"
#include "helper/signal_analysis.h"
#include "helper/vga.h"
#include "fft_helper/kiss_fftr.h"
#include "model/model1.h"

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

#define KEY_R               0x2D
#define KEY_P               0x4D
#define KEY_LEFT            0x6B
#define KEY_RIGHT           0x74

int time_plot_line_heights[STANDARD_GRAPH_WIDTH/2] = {0};
int const samples_per_pixel = (RECORDING_LENGTH * 2) / STANDARD_GRAPH_WIDTH;
point const time_plot_mid_left = {25, 160};
int const axes_offset = 2;

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
volatile int * pixel_ctrl_ptr = (int *)VGA_BASE;

volatile char* character_buffer_start;
volatile int * character_ctrl_ptr = (int *)CHARACTER_BASE;

/*
DISPLAY GRAPH LEGEND
0-Time Domain
1-Magnitude Spectrum
2-Spectrogram
*/
int DISPLAY_GRAPH = 0;
int PREV_DISPLAY_GRAPH = 0;
int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH];

// Changed from double to float — soft-float emulation of 32-bit ops is
// substantially cheaper than 64-bit on the NiosV rv32im target
float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS];
float average_fft[NO_FREQ_BINS];
float frequency_bins[NO_FREQ_BINS];
float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS];

bool record = false;
bool playback = false;

bool time_fill = true;
bool spectrum_fill = false;
bool spectrogram_fill = false;

int captureRecordingAndGraphTime();

void playbackRecording();
void displayMagnitudeSpectrum();
void displayTime();
void displaySpectrogram();
void displayCorrectGraph();

const char* button1 = "Time";
const char* button2 = "Spectrum";
const char* button3 = "Spectrogram";

static inline bool ps2_read(unsigned char *out);

// Helper: draw buttons + graph into the CURRENT back buffer, then swap.
// Call this twice (once per buffer) whenever the display content changes.
static void drawFullFrame(
    const char* button1, const char* button2, const char* button3,
    bool time_fill, bool spectrum_fill, bool spectrogram_fill
){
    clearRegion((point){0, 80}, 320, 160);   // wipe buttons + graph region
    createGraphButton(button1, (point){25, 80},  time_fill,        GRAPH_COLOR);
    createGraphButton(button2, (point){55, 80},  spectrum_fill,    GRAPH_COLOR);
    createGraphButton(button3, (point){100, 80}, spectrogram_fill, GRAPH_COLOR);
    displayCorrectGraph();
}

int main(void){
    character_buffer_start = (volatile char*) *character_ctrl_ptr;  // add this back
    *(pixel_ctrl_ptr + 1) = (int) &Buffer1;
    pixel_buffer_start = *pixel_ctrl_ptr;

    compute_frequency_bins(frequency_bins);
    compute_mel_filterbank(filterbank, 80.0f, 4000.0f);

    createGraphButton(button1, (point){25, 80}, time_fill, GRAPH_COLOR);
    createGraphButton(button2, (point){55, 80}, spectrum_fill, GRAPH_COLOR);
    createGraphButton(button3, (point){100, 80}, spectrogram_fill, GRAPH_COLOR);

    clearRegion((point){0, 95}, 320, 145);
    displayCorrectGraph();

    static bool ps2_break_pending = false;
    static bool ps2_extend_pending = false;

    // PS/2 Keyboard Polling Loop
    while (1){
        unsigned char byte;

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
                if (byte == KEY_R) record = true;
                if (byte == KEY_P) playback = true;

                if (is_extended && byte == KEY_LEFT)  DISPLAY_GRAPH = ((DISPLAY_GRAPH - 1) + 3) % 3;
                if (is_extended && byte == KEY_RIGHT) DISPLAY_GRAPH = (DISPLAY_GRAPH + 1) % 3;
            }
        }

        if (DISPLAY_GRAPH != PREV_DISPLAY_GRAPH){

            if (PREV_DISPLAY_GRAPH == 2){
                point spectrogram_top_left = {25, 100};
                clearSpectrogramLabel(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40);
            }

            //compute button fill state BEFORE any drawing,
            //so buttons and graph are always in sync
            fillComparator(DISPLAY_GRAPH, &time_fill, &spectrum_fill, &spectrogram_fill);

            drawFullFrame(button1, button2, button3,
                        time_fill, spectrum_fill, spectrogram_fill);

            PREV_DISPLAY_GRAPH = DISPLAY_GRAPH;
        }

        if (record){
            record = false;
            *led_ptr = 0x1;
            captureRecordingAndGraphTime();

            *led_ptr = 0x20;
            kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_LENGTH, 0, NULL, NULL);
            unzip_recording_into_frames(frame_array, recording);
            
            for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++) {
                compute_fft_magnitude(frame_array[frame_idx], fft_array[frame_idx], cfg);
            }

            free(cfg);
            compute_average_fft(fft_array, average_fft);
            drawFullFrame(button1, button2, button3,
                          time_fill, spectrum_fill, spectrogram_fill);

            *led_ptr = 0;

            // Neural network: classify each 0.5 s chunk, light its LED 
            FeatureVector1 fv1;
            float feature_vec[FEATURES_1];
            int led_result = 0;

            for (int chunk = 0; chunk < CHUNKS_PER_RECORDING; chunk++) {
                int start = chunk * FRAMES_PER_CHUNK;
                int end   = start + FRAMES_PER_CHUNK;

                create_feature_vector1_chunk(&fv1,
                                            frame_array,
                                            fft_array,
                                            frequency_bins,
                                            filterbank,
                                            start, end);
                flatten_feature_vector1(&fv1, feature_vec);

                int result = model1(feature_vec);
                if (result == 1)
                    led_result |= (1 << chunk);   // bit N = chunk N = LED N
            }

            *led_ptr = led_result;   // all 10 LEDs set in one write
        }

        if (playback){
            playback = false;
            *led_ptr = 0x2;
            playbackRecording();
            *led_ptr = 0;
        }

    }
}

int captureRecordingAndGraphTime() {
    if (DISPLAY_GRAPH != 0){
        DISPLAY_GRAPH = 0;
        fillComparator(0, &time_fill, &spectrum_fill, &spectrogram_fill);
        drawFullFrame(button1, button2, button3, time_fill, spectrum_fill, spectrogram_fill);
    }
    point graph_region = {15, 95};
    clearRegion(graph_region, 295, 155);
    int const usable_height = STANDARD_GRAPH_HEIGHT - 2 * axes_offset;
    int const MAX_AMPLITUDE = 0x6FFFFFFF;
    int x = time_plot_mid_left.x;
    int col_peak = 0;  // tracks peak within current pixel column
    
    // Draw directly to the front (currently displayed) buffer — no vsync stalls
    clearRegion(graph_region, 295, 155);

    drawLine((point){x, time_plot_mid_left.y + (STANDARD_GRAPH_HEIGHT/2) - axes_offset},
             (point){x, time_plot_mid_left.y - (STANDARD_GRAPH_HEIGHT/2) + axes_offset},
             LINE_COLOR, false);
    drawLine((point){x, time_plot_mid_left.y},
             (point){x + STANDARD_GRAPH_WIDTH, time_plot_mid_left.y},
             LINE_COLOR, false);

    for (int i = 0; i < RECORDING_LENGTH; i++) {
        if (audio_ptr->rarc > 0 && audio_ptr->ralc > 0) {
            recording[i] = audio_ptr->ldata;
            int absval = abs(recording[i]);
            if (absval > col_peak) col_peak = absval;

            if (i % samples_per_pixel == samples_per_pixel - 1) {
                int line_height = (int)(((float)col_peak / (float)MAX_AMPLITUDE) * usable_height);
                time_plot_line_heights[(x - time_plot_mid_left.x) / 2] = line_height;
                
                drawLine((point){x, time_plot_mid_left.y + line_height / 2},
                         (point){x, time_plot_mid_left.y - line_height / 2},
                         GRAPH_COLOR, false);
                x += 2;
                col_peak = 0;  // reset for next column
            }
        } else {
            i--;
        }

    }
}

static inline bool ps2_read(unsigned char *out) {
    int val = *keyboard_ptr;
    if (val & 0x8000) {
        *out = (unsigned char)(val & 0xFF);
        return true;
    }
    return false;
}

void playbackRecording(){
    //make current graph white
    for (int x = time_plot_mid_left.x; x < time_plot_mid_left.x + STANDARD_GRAPH_WIDTH; x += 2){
        drawLine((point){x, time_plot_mid_left.y + (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    (point){x, time_plot_mid_left.y - (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    LINE_COLOR, false);
    }

    int x = time_plot_mid_left.x;

    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->wsrc > 0 && audio_ptr->wslc > 0){
            audio_ptr->ldata = recording[i];
            audio_ptr->rdata = recording[i];
            if (i % samples_per_pixel == samples_per_pixel - 1){
                drawLine((point){x, time_plot_mid_left.y + (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    (point){x, time_plot_mid_left.y - (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    GRAPH_COLOR, false);
                x += 2;
            }
        }
        else i--;
    }
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
        RECORDING_LENGTH
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
        displayTime();
    }
}
