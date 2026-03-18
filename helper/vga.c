#include "vga.h"
#include <stdlib.h>   // for abs

// Character bitmap
static const unsigned char font[128][8] = {
    ['A'] = {0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['B'] = {0x7C, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00},
    ['C'] = {0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C, 0x00},

    ['0'] = {0x3C, 0x42, 0x46, 0x4A, 0x52, 0x62, 0x3C, 0x00},
    ['1'] = {0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x3E, 0x00},
    ['2'] = {0x3C, 0x42, 0x02, 0x0C, 0x30, 0x40, 0x7E, 0x00},

    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void plotPixel(point p, short int line_color)
{
    volatile short int *one_pixel_address;
    one_pixel_address = (volatile short int *)(pixel_buffer_start + (p.y << 10) + (p.x << 1));
    *one_pixel_address = line_color;
}

void clearScreen(void) {
    for (int x = 0; x < 320; x++) {
        for (int y = 0; y < 240; y++) {
            plotPixel((point){x, y}, 0x0000);
        }
    }
}

void waitForVsync(void)
{
    int status;
    *pixel_ctrl_ptr = 1;
    status = *(pixel_ctrl_ptr + 3);
    while ((status & 0x01) != 0) {
        status = *(pixel_ctrl_ptr + 3);
    }
}

void swapXY(point* p) {
    int temp = p->x;
    p->x = p->y;
    p->y = temp;
}

void swap2Points(point* p0, point* p1) {
    int temp = p0->x;
    p0->x = p1->x;
    p1->x = temp;

    temp = p0->y;
    p0->y = p1->y;
    p1->y = temp;
}

void drawLine(point p0, point p1, short int color) {
    bool is_steep = abs(p1.y - p0.y) > abs(p1.x - p0.x);

    if (is_steep) {
        swapXY(&p0);
        swapXY(&p1);
    }

    if (p0.x > p1.x) {
        swap2Points(&p0, &p1);
    }

    int delta_x = p1.x - p0.x;
    int delta_y = abs(p1.y - p0.y);
    int error = -delta_x / 2;
    int y = p0.y;
    int y_step = (p1.y > p0.y) ? 1 : -1;

    for (int x = p0.x; x <= p1.x; x++) {
        if (is_steep) {
            plotPixel((point){y, x}, color);
        } else {
            plotPixel((point){x, y}, color);
        }

        error = error + delta_y;
        if (error > 0) {
            y = y + y_step;
            error = error - delta_x;
        }
    }
}

void fillBox(point p, int size, short int color) {
    for (int x = p.x; x < p.x + size; x++) {
        for (int y = p.y; y < p.y + size; y++) {
            plotPixel((point){x, y}, color);
        }
    }
}

void drawChar(point top_left, char c, short int color, int scale) {
    unsigned char ch = (unsigned char)c;

    for (int row = 0; row < 8; row++) {
        unsigned char row_bits = font[ch][row];

        for (int col = 0; col < 8; col++) {
            if (row_bits & (1 << (7 - col))) {
                point pixel_pos = {
                    top_left.x + col * scale,
                    top_left.y + row * scale
                };
                fillBox(pixel_pos, scale, color);
            }
        }
    }
}

void drawText(point top_left, const char *text, short int color, int scale) {
    int cursor_x = top_left.x;

    while (*text) {
        drawChar((point){cursor_x, top_left.y}, *text, color, scale);
        cursor_x += 8 * scale;
        text++;
    }
}