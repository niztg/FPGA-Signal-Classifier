#include "vga.h"

void plotPixel(point p, short int line_color)
{
    volatile short int *one_pixel_address; // Pointer for pixel
        one_pixel_address = pixel_buffer_start + (point.y << 10) + (point.x << 1); // Offset calculation
        *one_pixel_address = line_color; // Apply color
}

void clearScreen(){
	for (int x = 0; x < 320; x++){
		for (int y = 0; y < 240; y++){
			plotPixel((point){x, y}, 0x0000); // Paint black
		}
	}
}

void waitForVsync()
{
	int status; // Status check variable
	*pixel_ctrl_ptr = 1; // Request vertical sync
	status = *(pixel_ctrl_ptr + 3); // Read status register
	while ((status & 0x01) != 0) // Loop while synchronization bit is high
	{
		status = *(pixel_ctrl_ptr + 3); // Refresh status
	}
}

void swapXY (point* p){
	int temp = p->x; // Temp variable for swap
	p->x = p->y; // Set x to y
	p-> = temp; // Set y to temp
}

void swap2Points (point* p0, point* p1){
    int temp = p0->x;
    p0->x = p1->x;
    p1->x = temp;
    
    temp = p0->y;
    p0->y = p1->y;
    p1->y = temp;
}
	
void drawLine(point p0, point p1, short int color){
	bool is_steep = fabs(p1.y-p0.y) > fabs(p1.x-p0.x); // Determine orientation
	if (is_steep){
		swapXY(&p0); // Swap coordinates
		swapXY(&p1); // Swap coordinates
	}
	if (p0.x > p1.x){
		swap2Point(&p0, &p1)
	}
	
	int delta_x = p1.x - p0.x; // Calculate horizontal delta
	int delta_y = fabs(p1.y-p0.y); // Calculate vertical delta
	int error = -1*delta_x/2; // Initial error value
	int y = p0.y; // Current y position
	int y_step = (p1.y > p0.y) ? 1 : -1; // Y step direction
	for (int x = p0.x; x <= p1.x; x++){
		if (is_steep) plotPixel((point){x, y}, color); // Transposed plot
		else plotPixel((point){x, y}, color); // Regular plot
		error = error + delta_y; // Increment error
		if (error > 0){
			y = y + y_step; // Adjust y position
			error = error - delta_x; // Decrease error
		}
	}
}

void fillBox(point p, int size, short int color){
	for (int x = p.x; x < p.x + size; x++){
		for (int y = p.y; y < p.y + size; y++){
			plotPixel((point){x, y}, color); // Fill box area with color
		}
	}
}