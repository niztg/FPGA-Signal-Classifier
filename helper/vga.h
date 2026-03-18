#ifndef VGA_H
#define VGA_H

#include <stdbool.h>
#include <math.h>

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
void drawLine(point p0, point p1, short int color);
void fillBox(point p, int size, short int color);

#endif
