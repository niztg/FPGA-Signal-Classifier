#include "vga.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "data_processing.h"

#define TEXT_CELL_H 4
#define TEXT_CELL_W 4

extern int time_plot_line_heights[];

// Hard-coded sin/cos of all angles 2πk/8
static const float RADAR_COS[8] = {
     1.0000f,  0.7071f,  0.0000f, -0.7071f,
    -1.0000f, -0.7071f,  0.0000f,  0.7071f
};
static const float RADAR_SIN[8] = {
     0.0000f, -0.7071f, -1.0000f, -0.7071f,
     0.0000f,  0.7071f,  1.0000f,  0.7071f
};

void vga_text(int x, int y, char * text_ptr) {
    int offset;
    offset = (y << 7) + x;
    while (*(text_ptr) != NULL) {
        *(character_buffer_start + offset) = *(text_ptr);
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

// Clears only the rectangular region of the screen that the graph occupies,
// rather than all 76,800 pixels. Cuts pixel write count by ~40%.
void clearRegion(point top_left, int width, int height) {
    for (int x = top_left.x; x < top_left.x + width; x++) {
        for (int y = top_left.y; y < top_left.y + height; y++) {
            volatile short int *addr = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
            *addr = BACKGROUND_COLOR;
        }
    }
    // Clear only the character cells that overlap the region
    int text_x_start = top_left.x / TEXT_CELL_W;
    int text_y_start = top_left.y / TEXT_CELL_H;
    int text_x_end   = (top_left.x + width)  / TEXT_CELL_W + 1;
    int text_y_end   = (top_left.y + height) / TEXT_CELL_H + 1;
    for (int y = text_y_start; y < text_y_end; y++) {
        for (int x = text_x_start; x < text_x_end; x++) {
            *(character_buffer_start + (y << 7) + x) = ' ';
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
            } else {
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

void drawGraphBoundingBox(point top_left, int graph_height, int graph_width){
    point bottom_left  = {top_left.x, top_left.y + graph_height};
    point top_right    = {top_left.x + graph_width, top_left.y};
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
    int left_x   = top_left.x + 1;
    int right_x  = top_left.x + graph_width - 2;
    int top_y    = top_left.y + 1;
    int bottom_y = top_left.y + graph_height - 2;

    int no_horizontal_lines = (no_horizontal_partitions > 1) ? (no_horizontal_partitions - 1) : 0;
    int no_vertical_lines   = (no_vertical_partitions   > 1) ? (no_vertical_partitions   - 1) : 0;

    if (no_horizontal_lines > 0) {
        int horizontal_y[no_horizontal_lines];
        for (int i = 0; i < no_horizontal_lines; i++) {
            horizontal_y[i] = top_y + ((i + 1) * (bottom_y - top_y)) / no_horizontal_partitions;
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
            vertical_x[i] = left_x + ((i + 1) * (right_x - left_x)) / no_vertical_partitions;
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

    int left_x = top_left.x;
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
        vga_text(pixelToTextX(label_pixel_x), pixelToTextY(label_pixel_y), label);
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

    int left_x   = top_left.x;
    int bottom_y = top_left.y + graph_height - 1;
    int tick_length = 4;

    (void)graph_width;

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
            snprintf(label, sizeof(label), "%.3g%s", scaled_y_value, scaled_units);
        } else {
            snprintf(label, sizeof(label), "%.3g", scaled_y_value);
        }

        int label_pixel_x = left_x - 6 - ((int)strlen(label) * TEXT_CELL_W);
        int label_pixel_y = y_pixel - TEXT_CELL_H / 2;
        vga_text(pixelToTextX(label_pixel_x), pixelToTextY(label_pixel_y), label);
    }
}

void plotTimeDomain(point reference, int width, int height,
    int number_of_samples){

    // draw axes
    drawLine((point){reference.x, reference.y + (height/2) - axes_offset},
             (point){reference.x, reference.y - (height/2) + axes_offset},
             LINE_COLOR, false);
    drawLine((point){reference.x, reference.y},
             (point){reference.x + width, reference.y},
             LINE_COLOR, false);

    int const usable_height = height - 2 * axes_offset;
    int x = reference.x;

    for (int x = reference.x; x < reference.x + width; x += 2) {
        int line_height = time_plot_line_heights[(x - reference.x) / 2];

        drawLine((point){x, reference.y + (line_height / 2)},
                 (point){x, reference.y - (line_height / 2)},
                 GRAPH_COLOR, false);
    }
    drawLine((point){reference.x, reference.y + (height/2) - axes_offset},
             (point){reference.x, reference.y - (height/2) + axes_offset},
             LINE_COLOR, false);
    drawLine((point){reference.x, reference.y},
             (point){reference.x + width, reference.y},
             LINE_COLOR, false);
}

void plotMagnitudeSpectrum(
    float average_fft[NO_FREQ_BINS],
    point top_left,
    int graph_width,
    int graph_height,
    short int color,
    short int fill_color
){
    if (NO_FREQ_BINS < 1 || graph_width <= 1 || graph_height <= 1) return;

    float max_value = get_max_value(average_fft, NO_FREQ_BINS);
    if (max_value <= 0.0f) return;

    float pixel_step = (float)(graph_width - 1) / (NO_FREQ_BINS - 1);
    int bottom_y = top_left.y + graph_height - 1;

    point prev_point = {
        top_left.x,
        bottom_y - (int)((average_fft[0] / max_value) * (graph_height - 1))
    };
    drawLine(prev_point, (point){prev_point.x, bottom_y}, fill_color, false);
    plotPixel(prev_point, color);

    for (int i = 1; i < NO_FREQ_BINS; i++) {
        float percent = average_fft[i] / max_value;
        point graph_point = {
            top_left.x + (int)(i * pixel_step),
            bottom_y - (int)(percent * (graph_height - 1))
        };

        // fill under this bin
        drawLine(graph_point, (point){graph_point.x, bottom_y}, fill_color, false);

        // connecting line along the top edge
        drawLine(prev_point, graph_point, color, false);

        prev_point = graph_point;
    }
}

short int magnitude_to_color(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    int r, g, b;

    if (v < 0.25f) {
        // black → dark purple
        float t = v / 0.25f;
        r = (int)(t * 4);
        g = (int)(t * 2);
        b = (int)(t * 12);
    } else if (v < 0.5f) {
        // dark purple → deep magenta/red
        float t = (v - 0.25f) / 0.25f;
        r = 4 + (int)(t * 16);    // 4 → 20
        g = 2 + (int)(t * 2);     // 2 → 4
        b = 12 - (int)(t * 4);    // 12 → 8
    } else if (v < 0.75f) {
        // deep magenta → orange
        float t = (v - 0.5f) / 0.25f;
        r = 20 + (int)(t * 11);   // 20 → 31
        g = 4 + (int)(t * 24);    // 4 → 28
        b = 8 - (int)(t * 8);     // 8 → 0
    } else {
        // orange → yellow/white
        float t = (v - 0.75f) / 0.25f;
        r = 31;
        g = 28 + (int)(t * 35);   // 28 → 63
        b = (int)(t * 12);        // 0 → 12
    }

    return (short int)((r << 11) | (g << 5) | b);
}

void plotSpectrogram(
    float fft_array[FRAMES_PER_RECORDING][NO_FREQ_BINS],
    point top_left,
    int graph_height,
    int graph_width
){
    /*
    COLOR MAPPING ("Magma" Color scheme, most commonly used in audio):
    Normalized Magnitude of FFT at given frequency   |  Color
    –––––––––––––––––––––––––––––––––––––––––––––––– | –––––––
    0.00                                             |   Black
    0.25                                             |   Dark Purple
    0.50                                             |   Red
    0.75                                             |   Orange
    1.00                                             |   Yellow/White
    */
    // Precompute max once
    float running_max_value = get_max_value(fft_array[0], NO_FREQ_BINS);
    for (int f = 1; f < FRAMES_PER_RECORDING; f++){
        float max_f = get_max_value(fft_array[f], NO_FREQ_BINS);
        if (max_f > running_max_value) running_max_value = max_f;
    }

    if (running_max_value <= 0.0f) return;

    float inv_max = 1.0f / running_max_value;  // multiply instead of divide per pixel

    int x_min = top_left.x + 1;
    int x_max = top_left.x + graph_width  - 2;
    int y_min = top_left.y + 1;
    int y_max = top_left.y + graph_height - 2;

    int x_range = x_max - x_min + 1;
    int y_range = y_max - y_min + 1;

    for (int px = x_min; px <= x_max; px++) {
        int f = (px - x_min) * FRAMES_PER_RECORDING / x_range;
        for (int py = y_min; py <= y_max; py++) {
            int k = (y_max - py) * NO_FREQ_BINS / y_range;
            plotPixel((point){px, py}, magnitude_to_color(fft_array[f][k] * inv_max));
        }
    }
}

void drawSpectrogramLabel(
    point top_left,
    int graph_height,
    int graph_width
){
    // snap all positions to the 4x4 character grid
    int bar_x      = ((top_left.x + graph_width + 8) / TEXT_CELL_W) * TEXT_CELL_W;
    int bar_top    = ((top_left.y + 8) / TEXT_CELL_H) * TEXT_CELL_H;
    int bar_bottom = ((top_left.y + graph_height - 8) / TEXT_CELL_H) * TEXT_CELL_H;
    int bar_height = bar_bottom - bar_top;
    int bar_width  = 16;

    // text lands exactly on grid rows that match bar top and bottom
    vga_text(bar_x / TEXT_CELL_W, (bar_top - 6) / TEXT_CELL_H,    "HIGH");
    vga_text((bar_x + 3) / TEXT_CELL_W, (bar_bottom + 6) / TEXT_CELL_H, "LOW");

    // draw the gradient bar
    for (int i = 0; i <= bar_height; i++){
        short int color = magnitude_to_color(1.0f - (float) ((float)i / (float)bar_height));
        for (int j = 0; j < bar_width; j++){
            plotPixel((point){bar_x + j, bar_top + i}, color);
        }
    }
}

void createGraphButton(
    const char* label,
    point top_left,
    bool fill,
    short int fill_color
){
    size_t length = strlen(label);

    int pad_x = 4;  // one cell of padding per side
    int pad_y = 4;

    int button_width  = 2 * pad_x + (length * 4);  // = 8 + length * 4
    int button_height = 2 * pad_y + 4;              // = 12

    drawGraphBoundingBox(top_left, button_height, button_width);

    if (fill){
        for (int x = top_left.x + 1; x < top_left.x + button_width; x++){
            int y_coord_top = top_left.y + 1;
            int y_coord_bottom = top_left.y + button_height - 1;
            drawLine(
                (point){x, y_coord_top},
                (point){x, y_coord_bottom},
                fill_color,
                false
            );
        }
    } else {
        // explicitly clear the interior to background
        for (int x = top_left.x + 1; x < top_left.x + button_width; x++){
            drawLine(
                (point){x, top_left.y + 1},
                (point){x, top_left.y + button_height - 1},
                BACKGROUND_COLOR,
                false
            );
        }
    }  

    vga_text((top_left.x + 4) / 4, (top_left.y + 8) / 4, label);
}

void fillComparator(
    int DISPLAY_GRAPH,
    bool* time_fill,
    bool* spectrum_fill,
    bool* spectrogram_fill
){
    if (DISPLAY_GRAPH == 0){
        *time_fill = true;
        *spectrum_fill = false;
        *spectrogram_fill = false;
    } else if (DISPLAY_GRAPH == 1){
        *time_fill = false;
        *spectrum_fill = true;
        *spectrogram_fill = false;
    } else if (DISPLAY_GRAPH == 2){
        *time_fill = false;
        *spectrum_fill = false;
        *spectrogram_fill = true;
    }
}

void drawResultBox(
    point top_left,
    int draw_index,
    short int fill_color,
    int box_width,
    int box_height
){
    int start_x = (top_left.x + 1) + draw_index * box_width;
    int y_coord = (top_left.y + 1);
    int end_x = (draw_index == CHUNKS_PER_RECORDING - 1)
                ? start_x + box_width - 1
                : start_x + box_width;
    for (int x = start_x; x < end_x; x++){
        drawLine(
            (point){x, y_coord},
            (point){x, y_coord + box_height - 2},
            fill_color,
            false
        );
    }
}

void drawFeatureBars(point top_left, int width, int height,
                     float values[4], const char* labels[4]) {

    static const float max_vals[4] = { 0.5f, 2500.0f, 1.0f, 0.65f };

    int label_cols     = 5;
    int value_cols     = 10;
    int label_px       = label_cols * TEXT_CELL_W;
    int value_px       = value_cols * TEXT_CELL_W;
    int bar_area_px    = width - label_px - value_px;
    int base_text_row  = pixelToTextY(top_left.y);
    int base_text_col  = pixelToTextX(top_left.x);
    int value_text_col = base_text_col + label_cols + pixelToTextX(bar_area_px) + 1;

    clearRegion(top_left, width, height);

    for (int i = 0; i < 4; i++) {
        int text_row = base_text_row + i;

        // lock bar position to the text grid
        int bar_y = text_row * TEXT_CELL_H + 1;
        int bar_h = TEXT_CELL_H - 2;
        int bar_x = top_left.x + label_px;

        vga_text(base_text_col, text_row, (char*)labels[i]);

        float normalized = values[i] / max_vals[i];
        if (normalized > 1.0f) normalized = 1.0f;
        int filled = (int)(normalized * bar_area_px);

        for (int x = bar_x; x < bar_x + filled; x++)
            drawLine((point){x, bar_y}, (point){x, bar_y + bar_h}, GRAPH_COLOR, false);

        if (values[i] != 0.0f) {
            char val_buf[12];
            if (i == 1)
                sprintf(val_buf, "%-9.1f", values[i]);
            else
                sprintf(val_buf, "%-9.4f", values[i]);
            vga_text(value_text_col, text_row, val_buf);
        }
    }
}

void plotMFCCRadar(
    float mfcc_mean[NUM_MFCC],
    float mfcc_std[NUM_MFCC],
    point centre,
    float radius,
    short int mean_color,
    short int std_color
){
    // find the maximum value of the mean
    float max_mean = 1.0f;
    float max_std = 1.0f;
    for (int i = 0; i < NUM_MFCC; i++){
        float mean_i = fabsf(mfcc_mean[i]);
        if (mean_i > max_mean) max_mean = mean_i;
    }

    for (int i = 0; i < NUM_MFCC; i++){
        float std_i = fabsf(mfcc_std[i]);
        if (std_i > max_std) max_std = std_i;
    }

    // add four dotted rings at 25%, 50%, 75% and 100% of the radius 
    for (int ring = 1; ring <= 4; ring++){
        // ring 1 = 0.25R
        // ring 2 = 0.5R
        // etc
        float r = 0.25f * ring * radius;
        for (int i = 0; i < NUM_MFCC; i++){
            float cos_angle = RADAR_COS[i];
            float sin_angle = RADAR_SIN[i];

            float cos_next_angle = RADAR_COS[i % NUM_MFCC];
            float sin_next_angle = RADAR_SIN[i % NUM_MFCC];
            
            point A = {
                centre.x + r * cos_angle,
                centre.y + r * sin_angle
            };
            point B = {
                centre.x + r * cos_next_angle,
                centre.y + r * sin_next_angle
            }

            drawLine(
                A, B,
                LINE_COLOR,
                true // dotted
            );
        }
    }

    // axes at each 2πk/8
    for (int k = 0; k < NUM_MFCC; k++){
        point tip = {
            centre.x + radius * RADAR_COS[k];
            centre.y + radius * RADAR_SIN[k];
        }
        drawLine(
            centre, tip,
            LINE_COLOR,
            true // also dotted
        );
    }

    // plot the standard deviations first
    for (int k = 0; k < NUM_MFCC){
        index next = k % NUM_MFCC;
        float std_radius_current = radius * (mfcc_std[k] / max_std);
        float std_radius_next = radius * (mfcc_std[next] / max_std);

        point current = {
            centre.x + std_radius_current * RADAR_COS[k],
            centre.y + std_radius_current * RADAR_SIN[k]
        };

        point next = {
            centre.x + std_radius_next * RADAR_COS[next],
            centre.y + std_radius_next * RADAR_SIN[next]
        };

        drawLine(
            current, next,
            std_color,
            false
        );

    }

    // now plot the means
    for (int k = 0; k < NUM_MFCC){
        index next = k % NUM_MFCC;
        float mean_radius_current = radius * (mfcc_mean[k] / max_std);
        float mean_radius_next = radius * (mfcc_mean[next] / max_std);

        point current = {
            centre.x + mean_radius_current * RADAR_COS[k],
            centre.y + mean_radius_next * RADAR_SIN[k]
        };

        point next = {
            centre.x + mean_radius_current * RADAR_COS[next],
            centre.y + mean_radius_next * RADAR_SIN[next]
        };

        drawLine(
            current, next,
            mean_color,
            false
        );

    }

    // Labels
    char label[3] = {'C', '0', '\0'};
    int label_r = radius + 6;
    for (int i = 0; i < NUM_MFCC; i++) {
        label[1] = '1' + i;
        int lx = centre.x + (int)(label_r * RADAR_COS[i]);
        int ly = centre.y + (int)(label_r * RADAR_SIN[i]);
        vga_text(pixelToTextX(lx), pixelToTextY(ly), label);
    }
}