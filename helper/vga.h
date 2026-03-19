#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <math.h>

#define BACKGROUND_COLOR 0x0000
#define LINE_COLOR       0xFFFF

typedef struct {
    int x;
    int y;
} point;

extern volatile char* character_buffer_start;
extern volatile int * character_ctrl_ptr;
extern volatile int pixel_buffer_start;
extern volatile int* pixel_ctrl_ptr;

#define RECORDING_LENGTH   40000      // total samples
#define FRAME_LENGTH       256        // samples per frame
#define HOP_SIZE           128        // overlap step
#define SAMPLING_RATE      8000       // Hz
#define FEATURES_0         12         // 12 features in the level 0 feature vector

#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

// Derived constants
#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)

// For real-valued signals: bins 0..N/2 inclusive
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)

// Frequency resolution (Hz per bin)
#define BIN_SPACING        ((double)SAMPLING_RATE / FRAME_LENGTH)

void vga_text(int x, int y, char * text_ptr);
void plotPixel(point p, short int line_color);
void clearScreen(void);
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
static int pixelToTextX(int pixel_x);
static int pixelToTextY(int pixel_y);
static void chooseSIScale(double max_value, const char *base_units, double *scale_factor, const char **scaled_units);
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

//reference is the left horizontal midpoint of the graph
//width and height correspond to the size of the bounding box
//refernce is the midpoint of the left edge of the bounding box
void plotTimeDomain(point reference, int width, int height, 
    /*char* x_label, char* y_label,*/ int number_of_samples 
    /*int y_partition_size*/, int max_sample_amplitude
);
void plotMagnitudeSpectrum(
	double average_fft[NO_FREQ_BINS],
	point top_left,
	int graph_width,
	int graph_height,
	short int color
);

#endif
