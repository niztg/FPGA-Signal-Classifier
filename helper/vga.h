#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <math.h>

typedef struct {
    int x;
    int y;
} point;

extern volatile int pixel_buffer_start;
extern volatile int * pixel_ctrl_ptr;

void plotPixel(point p, short int line_color);
void clearScreen(void);
void waitForVsync(void);
void swapXY(point* p);
void swap2Points(point* p0, point* p1);
void drawLine(point p0, point p1, short int color);
void fillBox(point p, int size, short int color);
void drawChar(point top_left, char c, short int color, int scale);
void drawText(point top_left, const char *text, short int color, int scale);

#endif