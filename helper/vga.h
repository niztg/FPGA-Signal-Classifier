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

#endif
