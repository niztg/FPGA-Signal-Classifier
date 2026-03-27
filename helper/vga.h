#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <math.h>

#define BACKGROUND_COLOR 0x0000
#define LINE_COLOR       0xFFFF
#define GRAPH_COLOR      0xFDE0

#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

typedef struct {
    int x;
    int y;
} point;

extern int const samples_per_pixel;
extern int const axes_offset;

extern volatile char* character_buffer_start;
extern volatile int * character_ctrl_ptr;
extern volatile int pixel_buffer_start;
extern volatile int* pixel_ctrl_ptr;
extern int recording[];



#define RECORDING_LENGTH   40000
#define FRAME_LENGTH       256
#define HOP_SIZE           128
#define SAMPLING_RATE      8000
#define NUM_MFCC           8

#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)
#define BIN_SPACING        ((float)SAMPLING_RATE / FRAME_LENGTH)

void vga_text(int x, int y, char * text_ptr);
void plotPixel(point p, short int line_color);
void clearRegion(point top_left, int width, int height);
void waitForVsync(void);
void swapXY(point* p);
void swap2Points(point* p0, point* p1);
void drawLine(point p0, point p1, short int color, bool dotted);
void fillBox(point p, int size, short int color);
void drawGraphBoundingBox(point top_left, int graph_height, int graph_width);
void drawGraphGrid(
    int no_horizontal_partitions,
    int no_vertical_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int partition_color,
    int dot_spacing
);
void drawXAxisLabels(
    int no_vertical_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int axis_color,
    double max_x,
    const char *x_units
);
void drawYAxisLabels(
    int no_horizontal_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int axis_color,
    double max_y,
    const char *y_units
);

void plotTimeDomain(point reference, int width, int height,
    int number_of_samples
);
void plotMagnitudeSpectrum(
    float average_fft[NO_FREQ_BINS],
    point top_left,
    int graph_width,
    int graph_height,
    short int color,
    short int fill_color
);
short int magnitude_to_color(float v);
void plotSpectrogram(
    float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS],
    point top_left,
    int graph_height,
    int graph_width
);
void drawSpectrogramLabel(point top_left, int graph_height, int graph_width);
void createGraphButton(
    const char* label,
    point top_left,
    bool fill,
    short int fill_color
);
void fillComparator(
    int DISPLAY_GRAPH,
    bool* time_fill,
    bool* spectrum_fill,
    bool* spectrogram_fill,
    bool* radar_fill
);
void drawResultBox(
    point top_left,
    int draw_index,
    short int fill_color,
    int box_width,
    int box_height
);
void drawFeatureBars(point top_left, int width, int height,
                     float values[4], const char* labels[4]);
void plotMFCCRadar(
    float mfcc_mean[NUM_MFCC],
    float mfcc_std[NUM_MFCC],
    point centre,
    float radius,
    short int mean_color,
    short int std_color,
    short int partition_color
);

#endif