#ifndef VGA_H
#define VGA_H

#define BACKGROUND_COLOR 0x528A //dark grey

typedef struct{
    x;
    y;
} point;

void plotPixel(point p, short int line_color);

void clearScreen();

void waitForVsync();

void swapXY (point* p);

void swap2Points (point* p0, point* p1);

void drawLine(point p0, point p1, short int color);

void fillBox(point p, int size, short int color);

#endif