/*
accelerator.v

This file contains the code for the Verilog-based accelerator that allows multiple neurons to be processed simultaneously, as opposed to the 
serialized nature of C.

NEURAL NET ARCHITECTURE:
FEATURE VECTOR[0:19] ---> LAYER 1 - 20 NEURONS (ReLU) ---> LAYER 2 - 12 NEURONS (ReLU) ---> PREDICTION[0]

MEMORY-MAPPED REGISTER ARCHITECTURE
Base Address: 0xFF210000

Offset (words)	|		Register
---------------|-----------------
0					|		ZCR
1					|		Spectral Centroid
2					|		Low Band Power Ratio
3					|		High Band Power Ratio
4-11				|		Mean MFCC1-8
12-19				|		Standard Deviation MFCC1-8
20					|		Control Register (bit 0: GO bit. GO <-- 1 : begin computation)
21					|		Status Register  (bit 0: DONE bit. when DONE == 0 : computation complete)
22					|		Result Register, stores signed Q4.12 value. Positive = target speaker.	
23-31				| 		Unused, but still reserved as Avalon can only reserve 2^n words in memory at a time.


The number format is Q4.12. This is a 16 bit two's-complement (programmer's choice, but two's-complement for us) integer which reserves the top 4 bits for the integer part and the bottom 12 bits for the fractional part.
It is found by multiplying the original number by 2^12=4096 and then interpreting the bits as described. Q4.12 numbers range from -8.0 to +7.997, with the top bit
representing the negative sign.

Multiplying two Q4.12 numbers returns a Q8.24 number. (RIP Kobe)

*/

module accelerator(
	input wire clk,
	input wire reset,
	input wire avs_address[4:0],
	input wire avs_read,
	input wire avs_write,
	input wire [31:0] avs_writedata,
	output reg [31:0] avs_readdata,
	output wire avs_waitrequest
);

	assign avs_waitrequest = 1'b0; // required to be here by Avalon, but unused
	
	localparam fractional_bits = 12; // in accordance with Q4.12 format
	
	// Neural Net Dimensions
	localparam L0_IN = 20;
	localparam L0_OUT = 20;
	localparam L1_IN = 32;
	localparam L1_OUT = 16;
	localparam L2_IN = 16;
	localparam L2_OUT = 1;
	
	
