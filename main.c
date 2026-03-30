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
#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

#define KEY_R               0x2D
#define KEY_P               0x4D
#define KEY_C               0x21
#define KEY_UP              0x75
#define KEY_DOWN            0x72
#define KEY_LEFT            0x6B
#define KEY_RIGHT           0x74

#define KEY_1 0x16
#define KEY_2 0x1E
#define KEY_3 0x26
#define KEY_4 0x25
#define KEY_5 0x2E
#define KEY_6 0x36
#define KEY_7 0x3D
#define KEY_8 0x3E
#define KEY_9 0x46
#define KEY_0 0x45

#define KEY_MINUS           0x4E
#define KEY_PLUS            0x55 

#define TEXT_CELL_H 4
#define TEXT_CELL_W 4

#define MAX_DISPLAY_BINS    NO_FREQ_BINS+(7*20)
#define MIN_DISPLAY_BINS    NO_FREQ_BINS-(6*20)

int* time_plot_line_heights = NULL;
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
2-MFCC Radar
3-Spectrogram
*/
int DISPLAY_GRAPH = 0;
int PREV_DISPLAY_GRAPH = 0;

int DISPLAY_CHUNK = 0;
int PREV_DISPLAY_CHUNK = 0;

int NO_DISPLAY_BINS = NO_FREQ_BINS;
int SPECTRUM_VIEWPORT_START = 0;

float frequency_bins[NO_FREQ_BINS];
float filterbank[NUM_MEL_FILTERS][NO_FREQ_BINS];

bool record = false;
bool playback = false;

bool time_fill = true;
bool spectrum_fill = false;
bool spectrogram_fill = false;
bool radar_fill = false;

bool spectrum_scale_change = false;

int captureRecordingAndGraphTime();

void playbackRecording();
void displayMagnitudeSpectrum();
void displayTime();
void displaySpectrogram();
void displayMFCCRadar();
void drawChunkData();
void displayCorrectGraph();

const char* button1 = "Time";
const char* button2 = "Spectrum";
const char* button3 = "MFCC Radar";
const char* button4 = "Spectrogram";

typedef struct {
    int frame_array[FRAMES_PER_RECORDING][FRAME_LENGTH];
    float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS];
    short int fill_color;
    short int std_color;
    bool has_been_run;
    int result_buffer[CHUNKS_PER_RECORDING];
    int recording[RECORDING_LENGTH];
    float average_fft[MAX_DISPLAY_BINS];
    FeatureVector1 feature_vector_array[CHUNKS_PER_RECORDING];
    int time_plot_line_heights[STANDARD_GRAPH_WIDTH/2];
} Channel;

Channel CHANNEL_1 = {
    {},
    {},
    0xFDE0,
    0xFD00,
    false,
    {},
    {},
    {},
    {},
    {}
};

Channel CHANNEL_2 = {
    {},
    {},
    0xF81F,
    0x8010,
    false,
    {},
    {},
    {},
    {},
    {}
};

Channel* ACTIVE_CHANNEL = &CHANNEL_1;
bool is_channel_1 = true;

static inline bool ps2_read(unsigned char *out);

// Helper: draw buttons + graph into the CURRENT back buffer, then swap.
// Call this twice (once per buffer) whenever the display content changes.
static void drawFullFrame(
    const char* button1, const char* button2, const char* button3, const char* button4,
    bool time_fill, bool spectrum_fill, bool spectrogram_fill, bool radar_fill
){
    clearRegion((point){0, 0}, 320, 240);   // wipe entire display
    createGraphButton(button1, (point){25, 76},  time_fill,        ACTIVE_CHANNEL -> fill_color);
    createGraphButton(button2, (point){56, 76},  spectrum_fill,    ACTIVE_CHANNEL -> fill_color);
    createGraphButton(button3, (point){102, 76}, radar_fill,       ACTIVE_CHANNEL -> fill_color);
    createGraphButton(button4, (point){155, 76}, spectrogram_fill, ACTIVE_CHANNEL -> fill_color);
    displayCorrectGraph();

    if (ACTIVE_CHANNEL -> has_been_run){
        // Redraw result boxes from stored result_buffer
        drawGraphBoundingBox((point){25, 58}, 12, 130);
        int no_greens = 0, no_reds = 0;
        for (int i = 0; i < CHUNKS_PER_RECORDING; i++){
            short int box_color = ACTIVE_CHANNEL->result_buffer[i] ? 0x0680 : 0xC000;
            if (ACTIVE_CHANNEL->result_buffer[i]){
                drawResultBox((point){25, 58}, no_greens, box_color, 13, 12);
                no_greens++;
            } else {
                drawResultBox((point){25, 58}, 9 - no_reds, box_color, 13, 12);
                no_reds++;
            }
        }

        drawChunkData(DISPLAY_CHUNK);

        float percent = (float)no_greens / CHUNKS_PER_RECORDING;
        char prediction_text[64];
        if (percent > 0.5f){
            sprintf(prediction_text, "Prediction: AUTHORIZED. CONFIDENCE: %.2f%%        ", percent * 100);
        } else {
            sprintf(prediction_text, "Prediction: NOT AUTHORIZED. CONFIDENCE: %.2f%%    ", (1 - percent) * 100);
        }
        vga_text(12, 12, prediction_text);
    }

    if (is_channel_1){
        vga_text(12, 19, "CH. 1");
    } else{
        vga_text(12, 19, "CH. 2");
    }
}

int main(void){
    character_buffer_start = (volatile char*) *character_ctrl_ptr;  // add this back
    *(pixel_ctrl_ptr + 1) = (int) &Buffer1;
    pixel_buffer_start = *pixel_ctrl_ptr;

    time_plot_line_heights = ACTIVE_CHANNEL->time_plot_line_heights;

    compute_frequency_bins(frequency_bins);
    compute_mel_filterbank(filterbank, 80.0f, 4000.0f);

    clearRegion((point){0, 0}, 320, 240);

    createGraphButton(button1, (point){25, 76},  time_fill,        ACTIVE_CHANNEL -> fill_color);
    createGraphButton(button2, (point){56, 76},  spectrum_fill,    ACTIVE_CHANNEL -> fill_color);
    createGraphButton(button3, (point){102, 76}, radar_fill,       ACTIVE_CHANNEL -> fill_color);
    createGraphButton(button4, (point){155, 76}, spectrogram_fill, ACTIVE_CHANNEL -> fill_color);

    displayCorrectGraph();

    vga_text(6, 19, "CH. 1");

    static bool ps2_break_pending = false;
    static bool ps2_extend_pending = false;

    *led_ptr = 0; // clear from previous execute

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
                if (byte == KEY_C){
                    if (is_channel_1){
                        ACTIVE_CHANNEL = &CHANNEL_2;
                        is_channel_1 = false;

                    } else {
                        ACTIVE_CHANNEL = &CHANNEL_1;
                        is_channel_1 = true;
                    }
                    time_plot_line_heights = ACTIVE_CHANNEL->time_plot_line_heights;
                
                drawFullFrame(button1, button2, button3, button4,
                    time_fill, spectrum_fill, spectrogram_fill, radar_fill);
                }

                if (byte == KEY_R) record = true;
                if (byte == KEY_P) playback = true;

                if (DISPLAY_GRAPH == 1 && byte == KEY_MINUS){
                    NO_DISPLAY_BINS += 20;
                    if (NO_DISPLAY_BINS > MAX_DISPLAY_BINS){
                        NO_DISPLAY_BINS = MAX_DISPLAY_BINS;
                    }
                    spectrum_scale_change = true;
                }

                if (DISPLAY_GRAPH == 1 && byte == KEY_PLUS){
                    NO_DISPLAY_BINS -= 20;
                    if (NO_DISPLAY_BINS < MIN_DISPLAY_BINS){
                        NO_DISPLAY_BINS = MIN_DISPLAY_BINS;
                    }
                    spectrum_scale_change = true;
                }

                if (DISPLAY_GRAPH == 1 && is_extended && byte == KEY_UP){
                    SPECTRUM_VIEWPORT_START -= 10;
                    if (SPECTRUM_VIEWPORT_START < 0) SPECTRUM_VIEWPORT_START = 0;
                    spectrum_scale_change = true;
                }

                if (DISPLAY_GRAPH == 1 && is_extended && byte == KEY_DOWN){
                    SPECTRUM_VIEWPORT_START += 10;
                    int max_start = NO_FREQ_BINS - NO_DISPLAY_BINS;
                    if (max_start < 0) max_start = 0;
                    if (SPECTRUM_VIEWPORT_START > max_start)
                        SPECTRUM_VIEWPORT_START = max_start;
                    spectrum_scale_change = true;
                }

                if (is_extended && byte == KEY_LEFT)  DISPLAY_GRAPH = ((DISPLAY_GRAPH - 1) + 4) % 4;
                if (is_extended && byte == KEY_RIGHT) DISPLAY_GRAPH = (DISPLAY_GRAPH + 1) % 4;

                if (byte == KEY_1) DISPLAY_CHUNK = 0;
                if (byte == KEY_2) DISPLAY_CHUNK = 1;
                if (byte == KEY_3) DISPLAY_CHUNK = 2;
                if (byte == KEY_4) DISPLAY_CHUNK = 3;
                if (byte == KEY_5) DISPLAY_CHUNK = 4;
                if (byte == KEY_6) DISPLAY_CHUNK = 5;
                if (byte == KEY_7) DISPLAY_CHUNK = 6;
                if (byte == KEY_8) DISPLAY_CHUNK = 7;
                if (byte == KEY_9) DISPLAY_CHUNK = 8;
                if (byte == KEY_0) DISPLAY_CHUNK = 9;
            }
        }

        if (DISPLAY_GRAPH != PREV_DISPLAY_GRAPH){
            //compute button fill state BEFORE any drawing,
            //so buttons and graph are always in sync
            fillComparator(DISPLAY_GRAPH, &time_fill, &spectrum_fill, &spectrogram_fill, &radar_fill);
            drawFullFrame(button1, button2, button3, button4,
                        time_fill, spectrum_fill, spectrogram_fill, radar_fill);

            PREV_DISPLAY_GRAPH = DISPLAY_GRAPH;
        }

        if (spectrum_scale_change){
            fillComparator(DISPLAY_GRAPH, &time_fill, &spectrum_fill, &spectrogram_fill, &radar_fill);
            drawFullFrame(button1, button2, button3, button4,
                        time_fill, spectrum_fill, spectrogram_fill, radar_fill);
            spectrum_scale_change = false;
        }

        if (DISPLAY_CHUNK != PREV_DISPLAY_CHUNK){
            drawFullFrame(button1, button2, button3, button4,
                        time_fill, spectrum_fill, spectrogram_fill, radar_fill);
            PREV_DISPLAY_CHUNK = DISPLAY_CHUNK;
        }

        if (record){
            ACTIVE_CHANNEL -> has_been_run = true;
            clearRegion((point){0,0}, 320, 75);

            record = false;
            *led_ptr = 0;
            captureRecordingAndGraphTime();
            kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_LENGTH, 0, NULL, NULL);
            unzip_recording_into_frames(ACTIVE_CHANNEL -> frame_array, ACTIVE_CHANNEL -> recording);

            // Hard code the exact body of the drawChunkData function once inline
            int chunk_idx = 0;
            float bar_values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            const char* bar_labels[4] = { "ZCR ", "SC  ", "LBPR", "HBPR" };

            drawGraphBoundingBox((point){25, 58}, 12, 130);
            vga_text(6, 4, "Chunk 0 / 10     ");
            drawFeatureBars((point){24, 24}, 200, 20, bar_values, bar_labels, ACTIVE_CHANNEL->std_color);
            vga_text(6, 12, "Prediction: --                  ");

            int no_reds = 0;
            int no_greens = 0;

            // Zero-pad the full array so bins beyond NO_FREQ_BINS render as flat
            for (int i = 0; i < MAX_DISPLAY_BINS; i++) ACTIVE_CHANNEL -> average_fft[i] = 0.0f;

            for (int frame_idx = 0; frame_idx < FRAMES_PER_RECORDING; frame_idx++){
                compute_fft_magnitude(ACTIVE_CHANNEL -> frame_array[frame_idx], ACTIVE_CHANNEL -> fft_array[frame_idx], cfg);
                for (int j = 0; j < NO_FREQ_BINS; j++) ACTIVE_CHANNEL -> average_fft[j] += ACTIVE_CHANNEL -> fft_array[frame_idx][j];

                if ((frame_idx + 1) % FRAMES_PER_CHUNK == 0){
                    int start = chunk_idx * FRAMES_PER_CHUNK;
                    int end   = start + FRAMES_PER_CHUNK;

                    FeatureVector1 fv;
                    float feature_vec[FEATURES_1];
                    create_feature_vector1_chunk(&fv, ACTIVE_CHANNEL->frame_array, ACTIVE_CHANNEL->fft_array,
                                                frequency_bins, filterbank, start, end);
                    flatten_feature_vector1(&fv, feature_vec);
                    ACTIVE_CHANNEL -> feature_vector_array[chunk_idx] = fv;

                    drawChunkData(chunk_idx);

                    int result = model1(feature_vec);
                    short int box_color = result ? 0x0680 : 0xC000;
                                        
                    ACTIVE_CHANNEL -> result_buffer[chunk_idx] = result;

                    if (result){
                        drawResultBox((point){25, 58}, no_greens, box_color, 13, 12);
                        no_greens++;
                    } else {
                        drawResultBox((point){25, 58}, 9 - no_reds, box_color, 13, 12);
                        no_reds++;
                    }

                    const char* running_prediction = (no_greens > no_reds)
                        ? "Prediction: AUTHORIZED                          "
                        : "Prediction: NOT AUTHORIZED                      ";
                    vga_text(6, 12, running_prediction);

                    *led_ptr |= result << chunk_idx;
                    chunk_idx++;
                }
            }

            float percent = (float) no_greens / 10;
            char prediction_text[64];

            if (percent > 0.5f){
                sprintf(prediction_text, "Prediction: AUTHORIZED. CONFIDENCE: %.2f%%        ", percent * 100);
            } else {
                sprintf(prediction_text, "Prediction: NOT AUTHORIZED. CONFIDENCE: %.2f%%    ", (1 - percent) * 100);
            }

            vga_text(6, 12, prediction_text);

            free(cfg);
            for (int i = 0; i < NO_FREQ_BINS; i++) ACTIVE_CHANNEL -> average_fft[i] /= FRAMES_PER_RECORDING;
            drawFullFrame(button1, button2, button3, button4,
                        time_fill, spectrum_fill, spectrogram_fill, radar_fill);
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
        fillComparator(0, &time_fill, &spectrum_fill, &spectrogram_fill, &radar_fill);
        drawFullFrame(button1, button2, button3, button4,
                        time_fill, spectrum_fill, spectrogram_fill, radar_fill);
    }
    point graph_region = {15, 93};
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
            ACTIVE_CHANNEL -> recording[i] = audio_ptr->ldata;
            int absval = abs(ACTIVE_CHANNEL -> recording[i]);
            if (absval > col_peak) col_peak = absval;

            if (i % samples_per_pixel == samples_per_pixel - 1) {
                int line_height = (int)(((float)col_peak / (float)MAX_AMPLITUDE) * usable_height);
                time_plot_line_heights[(x - time_plot_mid_left.x) / 2] = line_height;
                
                drawLine((point){x, time_plot_mid_left.y + line_height / 2},
                         (point){x, time_plot_mid_left.y - line_height / 2},
                         ACTIVE_CHANNEL -> fill_color, false);
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
    if (DISPLAY_GRAPH != 0){
        DISPLAY_GRAPH = 0;
        fillComparator(0, &time_fill, &spectrum_fill, &spectrogram_fill, &radar_fill);
        drawFullFrame(button1, button2, button3, button4,
                        time_fill, spectrum_fill, spectrogram_fill, radar_fill);
    }
    //make current graph white
    for (int x = time_plot_mid_left.x; x < time_plot_mid_left.x + STANDARD_GRAPH_WIDTH; x += 2){
        drawLine((point){x, time_plot_mid_left.y + (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    (point){x, time_plot_mid_left.y - (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    LINE_COLOR, false);
    }

    int x = time_plot_mid_left.x;

    for (int i = 0; i < RECORDING_LENGTH; i++){
        if (audio_ptr->wsrc > 0 && audio_ptr->wslc > 0){
            audio_ptr->ldata = ACTIVE_CHANNEL -> recording[i];
            audio_ptr->rdata = ACTIVE_CHANNEL -> recording[i];
            if (i % samples_per_pixel == samples_per_pixel - 1){
                drawLine((point){x, time_plot_mid_left.y + (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    (point){x, time_plot_mid_left.y - (time_plot_line_heights[(x - time_plot_mid_left.x)/2]/2)},
                    ACTIVE_CHANNEL -> fill_color, false);
                x += 2;
            }
        }
        else i--;
    }
}

void displayMagnitudeSpectrum(){
    point bode_plot_top_left = {25, 100};
    const char* x_axis_units = "Hz";

    float x_min_hz = SPECTRUM_VIEWPORT_START * BIN_SPACING;
    float x_max_hz = (SPECTRUM_VIEWPORT_START + NO_DISPLAY_BINS - 1) * BIN_SPACING;

    drawGraphBoundingBox(bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH);
    drawGraphGrid(5, 7, bode_plot_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH, 0x39E7, 3);
    drawXAxisLabels(
        5,
        bode_plot_top_left,
        STANDARD_GRAPH_HEIGHT,
        STANDARD_GRAPH_WIDTH,
        0xFFFF,
        (double)x_min_hz,
        (double)x_max_hz,
        x_axis_units
    );

    plotMagnitudeSpectrum(
        ACTIVE_CHANNEL -> average_fft,
        bode_plot_top_left,
        STANDARD_GRAPH_WIDTH,
        STANDARD_GRAPH_HEIGHT,
        ACTIVE_CHANNEL -> fill_color,
        ACTIVE_CHANNEL -> std_color,
        NO_DISPLAY_BINS,
        SPECTRUM_VIEWPORT_START
    );
}

void displayTime(){
    point time_plot_mid_left = {25, 160};

    plotTimeDomain(time_plot_mid_left,
        STANDARD_GRAPH_WIDTH,
        STANDARD_GRAPH_HEIGHT,
        RECORDING_LENGTH,
        ACTIVE_CHANNEL -> fill_color
    );
}

void displaySpectrogram(){
    point spectrogram_top_left = {25, 100};
    drawGraphBoundingBox(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40);
    drawXAxisLabels(10, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40, 0xFFFF, 0.0, 5.0, "s");
    drawYAxisLabels(5, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40, 0xFFFF, (double)frequency_bins[NO_FREQ_BINS-1], "Hz");
    drawSpectrogramLabel(spectrogram_top_left, STANDARD_GRAPH_HEIGHT, STANDARD_GRAPH_WIDTH - 40);
    plotSpectrogram(ACTIVE_CHANNEL -> fft_array, spectrogram_top_left, STANDARD_GRAPH_HEIGHT, 230);
}

void displayMFCCRadar(){
    point centre = {160, 160};
    float radius = 52.0f;

    // clear the label area first so stale text doesn't persist between chunks
    clearRegion((point){0, 90}, 320, 12);

    char chunk_label[48];
    if (ACTIVE_CHANNEL -> has_been_run){
        const char* verdict = ACTIVE_CHANNEL -> result_buffer[DISPLAY_CHUNK] ? "AUTHORIZED" : "NOT AUTHORIZED";
        sprintf(chunk_label, "Chunk %d: %s", DISPLAY_CHUNK + 1, verdict);
    } else {
        sprintf(chunk_label, "No recording yet.");
    }
    vga_text(25 / TEXT_CELL_W, 96 / TEXT_CELL_H, chunk_label);

    plotMFCCRadar(
        ACTIVE_CHANNEL -> feature_vector_array[DISPLAY_CHUNK].mfcc_mean,
        ACTIVE_CHANNEL -> feature_vector_array[DISPLAY_CHUNK].mfcc_std,
        centre,
        radius,
        ACTIVE_CHANNEL -> fill_color,
        ACTIVE_CHANNEL -> std_color,
        0x39E7
    );
}

void drawChunkData(
    int chunk_idx
){
    clearRegion((point){0,0}, 340, 44);

    char classification_text[40];
    const char* bar_labels[4] = { "ZCR ", "SC  ", "LBPR", "HBPR" };
    float bar_values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    sprintf(classification_text, "Chunk %d / %d     ", chunk_idx + 1, CHUNKS_PER_RECORDING);
    vga_text(6, 4, classification_text);

    bar_values[0] = ACTIVE_CHANNEL -> feature_vector_array[chunk_idx].zeroCrossingRate;
    bar_values[1] = ACTIVE_CHANNEL -> feature_vector_array[chunk_idx].spectralCentroid;
    bar_values[2] = ACTIVE_CHANNEL -> feature_vector_array[chunk_idx].lowBandPowerRatio;
    bar_values[3] = ACTIVE_CHANNEL -> feature_vector_array[chunk_idx].highBandPowerRatio;

    drawFeatureBars((point){24, 24}, 200, 20, bar_values, bar_labels, ACTIVE_CHANNEL -> std_color);
}

void displayCorrectGraph(){
    if (DISPLAY_GRAPH == 0){
        displayTime();
    } else if (DISPLAY_GRAPH == 1){
        displayMagnitudeSpectrum();
    } else if (DISPLAY_GRAPH == 3){
        displaySpectrogram();
    } else if (DISPLAY_GRAPH == 2){
        displayMFCCRadar();
    } else {
        displayTime();
    }
}