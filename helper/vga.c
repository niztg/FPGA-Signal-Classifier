#include "vga.h"
#include <stdlib.h>   // for abs

void vga_text(int x, int y, char * text_ptr) {
    int offset;
    /* assume that the text string fits on one line */
    offset = (y << 7) + x;
    while (*(text_ptr) != NULL) {
        *(character_buffer_start + offset) = *(text_ptr); // write to the character buffer
        ++text_ptr;
        ++offset;
    }
}

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

void drawLine(point p0, point p1, short int color, bool dotted) {
    bool is_steep = abs(p1.y - p0.y) > abs(p1.x - p0.x);
	int dash_buffer = 4;

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

	short int draw_color = color;

    for (int x = p0.x; x <= p1.x; x++) {
		if (dotted){
			if (dash_buffer % 4 == 0){
				draw_color = color;
			} else{
				draw_color = 0x0000;
			}
			dash_buffer++;
		}
        if (is_steep) {
            plotPixel((point){y, x}, draw_color);
        } else {
            plotPixel((point){x, y}, draw_color);
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

// Draws a bounding box for the graphs
void drawGraphBoundingBox(point top_left, int graph_height, int graph_width){
	point bottom_left = {top_left.x, top_left.y + graph_height};
	point top_right = {top_left.x + graph_width, top_left.y};
	point bottom_right = {top_left.x + graph_width, top_left.y + graph_height};

	drawLine(top_left, top_right, 0xFFFF, false);
	drawLine(top_left, bottom_left, 0xFFFF, false);
	drawLine(bottom_right, top_right, 0xFFFF, false);
	drawLine(bottom_right, bottom_left, 0xFFFF, false);
}

void drawGraphPartitions(
    int no_horizontal_partitions,
    int no_vertical_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int partition_color
){
    int left_x   = top_left.x;
    int right_x  = top_left.x + graph_width - 1;
    int top_y    = top_left.y;
    int bottom_y = top_left.y + graph_height - 1;

    // stay strictly inside the border
    int inner_left_x   = left_x + 1;
    int inner_right_x  = right_x - 1;
    int inner_top_y    = top_y + 1;
    int inner_bottom_y = bottom_y - 1;

    int dot_spacing = 6;   // adjust to taste

    // Draw horizontal partitions
    if (no_horizontal_partitions > 1) {
        for (int i = 1; i < no_horizontal_partitions; i++) {
            int y = top_y + (i * (graph_height - 1)) / no_horizontal_partitions;

            if (y <= inner_top_y || y >= inner_bottom_y) {
                continue;
            }

            for (int x = inner_left_x; x <= inner_right_x; x++) {
                // global dot lattice anchored to top-left of graph interior
                if (((x - inner_left_x) % dot_spacing) == 0) {
                    plotPixel((point){x, y}, partition_color);
                }
            }
        }
    }

    // Draw vertical partitions
    if (no_vertical_partitions > 1) {
        for (int i = 1; i < no_vertical_partitions; i++) {
            int x = left_x + (i * (graph_width - 1)) / no_vertical_partitions;

            if (x <= inner_left_x || x >= inner_right_x) {
                continue;
            }

            for (int y = inner_top_y; y <= inner_bottom_y; y++) {
                // same global dot lattice anchored to same origin
                if (((y - inner_top_y) % dot_spacing) == 0) {
                    plotPixel((point){x, y}, partition_color);
                }
            }
        }
    }
}
