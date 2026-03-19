#include "vga.h"
#include <stdlib.h>   // for abs
#include <stdio.h>
#include <string.h>

#include "data_processing.h"

#define TEXT_CELL_H 4
#define TEXT_CELL_W 4

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
            plotPixel((point){x, y}, BACKGROUND_COLOR);
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
				draw_color = BACKGROUND_COLOR;
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

	drawLine(top_left, top_right, LINE_COLOR, false);
	drawLine(top_left, bottom_left, LINE_COLOR, false);
	drawLine(bottom_right, top_right, LINE_COLOR, false);
	drawLine(bottom_right, bottom_left, LINE_COLOR, false);
}

void drawGraphGrid(
    int no_horizontal_partitions,
    int no_vertical_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int partition_color,
    int dot_spacing
){
    int left_x = top_left.x + 1;
    int right_x = top_left.x + graph_width - 2;
    int top_y = top_left.y + 1;
    int bottom_y = top_left.y + graph_height - 2;

    int no_horizontal_lines = (no_horizontal_partitions > 1) ? (no_horizontal_partitions - 1) : 0;
    int no_vertical_lines = (no_vertical_partitions > 1) ? (no_vertical_partitions - 1) : 0;

    if (no_horizontal_lines > 0) {
        int horizontal_y[no_horizontal_lines];

        for (int i = 0; i < no_horizontal_lines; i++) {
            horizontal_y[i] =
                top_y + ((i + 1) * (bottom_y - top_y)) / no_horizontal_partitions;
        }

        for (int h = 0; h < no_horizontal_lines; h++) {
            for (int x = left_x; x <= right_x; x += dot_spacing) {
                plotPixel((point){x, horizontal_y[h]}, partition_color);
            }
        }
    }

    if (no_vertical_lines > 0) {
        int vertical_x[no_vertical_lines];

        for (int i = 0; i < no_vertical_lines; i++) {
            vertical_x[i] =
                left_x + ((i + 1) * (right_x - left_x)) / no_vertical_partitions;
        }

        for (int v = 0; v < no_vertical_lines; v++) {
            for (int y = top_y; y <= bottom_y; y += dot_spacing) {
                plotPixel((point){vertical_x[v], y}, partition_color);
            }
        }
    }
}

static int pixelToTextX(int pixel_x){
    if (pixel_x < 0) return 0;
    return pixel_x / TEXT_CELL_W;
}

static int pixelToTextY(int pixel_y){
    if (pixel_y < 0) return 0;
    return pixel_y / TEXT_CELL_H;
}

// Function which determines the order of magnitude of a label and adds the appropriate SI multipler.
static void chooseSIScale(double max_value, const char *base_units,
                          double *scale_factor, const char **scaled_units){
    *scale_factor = 1.0;
    *scaled_units = base_units;

    if (base_units == NULL || base_units[0] == '\0'){
        *scaled_units = "";
        return;
    }

    if (strcmp(base_units, "Hz") == 0){
        if (max_value >= 1000000.0){
            *scale_factor = 1000000.0;
            *scaled_units = "MHz";
        } else if (max_value >= 1000.0){
            *scale_factor = 1000.0;
            *scaled_units = "kHz";
        } else {
            *scale_factor = 1.0;
            *scaled_units = "Hz";
        }
    } else if (strcmp(base_units, "V") == 0){
        if (max_value > 0.0 && max_value < 1.0){
            *scale_factor = 0.001;
            *scaled_units = "mV";
        } else {
            *scale_factor = 1.0;
            *scaled_units = "V";
        }
    }
}

// Draw x-axis tick marks and labels below the graph.
// Labels go from 0 to max_x.
void drawXAxisLabels(
    int no_vertical_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int axis_color,
    double max_x,
    const char *x_units
){
    if (no_vertical_partitions <= 0) return;

    int left_x = top_left.x + 1;
    int bottom_y = top_left.y + graph_height - 1;

    int tick_length = 4;
    int label_pixel_y = bottom_y + tick_length + 6;

    double scale_factor;
    const char *scaled_units;
    chooseSIScale(max_x, x_units, &scale_factor, &scaled_units);

    for (int i = 0; i <= no_vertical_partitions; i++){
        int x_pixel = left_x + (i * (graph_width - 1)) / no_vertical_partitions;
        double x_value = (i * max_x) / no_vertical_partitions;
        double scaled_x_value = x_value / scale_factor;

        for (int dy = 0; dy < tick_length; dy++){
            plotPixel((point){x_pixel, bottom_y + 1 + dy}, axis_color);
        }

        char label[32];
        if (scaled_units && scaled_units[0] != '\0'){
            snprintf(label, sizeof(label), "%.3g %s", scaled_x_value, scaled_units);
        } else {
            snprintf(label, sizeof(label), "%.3g", scaled_x_value);
        }

        int label_pixel_x = x_pixel - ((int)strlen(label) * TEXT_CELL_W) / 2;

        int text_x = pixelToTextX(label_pixel_x);
        int text_y = pixelToTextY(label_pixel_y);

        vga_text(text_x, text_y, label);
    }
}

void drawYAxisLabels(
    int no_horizontal_partitions,
    point top_left,
    int graph_height,
    int graph_width,
    short int axis_color,
    double max_y,
    const char *y_units
){
    if (no_horizontal_partitions <= 0) return;

    int left_x = top_left.x;
    int bottom_y = top_left.y + graph_height - 1;

    (void)graph_width;

    int tick_length = 4;

    double scale_factor;
    const char *scaled_units;
    chooseSIScale(max_y, y_units, &scale_factor, &scaled_units);

    for (int i = 0; i <= no_horizontal_partitions; i++){
        int y_pixel = bottom_y - (i * (graph_height - 1)) / no_horizontal_partitions;
        double y_value = (i * max_y) / no_horizontal_partitions;
        double scaled_y_value = y_value / scale_factor;

        for (int dx = 0; dx < tick_length; dx++){
            plotPixel((point){left_x - 1 - dx, y_pixel}, axis_color);
        }

        char label[32];
        if (scaled_units && scaled_units[0] != '\0'){
            snprintf(label, sizeof(label), "%.3g %s", scaled_y_value, scaled_units);
        } else {
            snprintf(label, sizeof(label), "%.3g", scaled_y_value);
        }

        int label_pixel_x = left_x - 6 - ((int)strlen(label) * TEXT_CELL_W);
        int label_pixel_y = y_pixel - TEXT_CELL_H / 2;

        int text_x = pixelToTextX(label_pixel_x);
        int text_y = pixelToTextY(label_pixel_y);

        vga_text(text_x, text_y, label);
    }
}

void plotTimeDomain(point reference, int width, int height,
    /*char* x_label, char* y_label,*/ int number_of_samples 
    /*int y_partition_size*/, int max_sample_amplitude){

    //plot the x and y axes with some offset with the bounding box
    int const axes_offset = 2;
    drawLine((point){reference.x + axes_offset, reference.y + (height/2) - axes_offset},(point) {reference.x + axes_offset, reference.y - (height/2) + axes_offset}, LINE_COLOR, false);
    drawLine((point){reference.x + axes_offset, reference.y}, (point){reference.x + width - axes_offset, reference.y}, LINE_COLOR, false);
    
    int sample_per_pixel = number_of_samples / (width - 2*axes_offset);

    int final_x = reference.x + width - axes_offset;

    int sample_index = 0;

    for (int x = reference.x + axes_offset; x < final_x; x += 2){
        int final_sample = sample_index + sample_per_pixel;
        int amplitude = 0;
        //loop through all the samples that are fit into one pixel and average them
        for (sample_index; sample_index < final_sample; sample_index++){
            amplitude += recording[sample_index];
        } 
        amplitude /= sample_per_pixel;
        amplitude = amplitude < 0 ? 0 : amplitude;

        int line_height = (int)(((float)amplitude / (float)max_sample_amplitude) * (height - 2*axes_offset));
        drawLine((point){x, reference.y + (line_height/2)}, (point){x, reference.y - (line_height/2)}, LINE_COLOR, false);
    }
}

void plotMagnitudeSpectrum(
    double average_fft[NO_FREQ_BINS],
    point top_left,
    int graph_width,
    int graph_height,
    short int color
){
    if (NO_FREQ_BINS < 1 || graph_width <= 1 || graph_height <= 1) {
        return;
    }

    double max_value = get_max_value(average_fft, NO_FREQ_BINS);
    if (max_value <= 0.0) {
        return;
    }

    double pixel_step = (double)(graph_width - 1) / (NO_FREQ_BINS - 1);

    point prev_point = {0, 0};
    bool first_point = true;

    for (int i = 0; i < NO_FREQ_BINS; i++) {
        double x_coordinate = top_left.x + i * pixel_step;

        double percent = average_fft[i] / max_value;
        double pixel_offset_from_bottom = percent * 0.9 * (graph_height - 1);

        double y_coordinate = (top_left.y + graph_height - 1) - pixel_offset_from_bottom;

        point graph_point = {(int)x_coordinate, (int)y_coordinate};

        fillBox(graph_point, 1, color);

        if (!first_point) {
            drawLine(prev_point, graph_point, color, false);
        }

        prev_point = graph_point;
        first_point = false;
    }
}
