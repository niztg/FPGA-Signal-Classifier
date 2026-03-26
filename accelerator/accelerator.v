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
