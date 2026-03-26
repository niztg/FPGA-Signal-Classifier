/*
accelerator.v

This file contains the code for the Verilog-based accelerator that allows multiple neurons to be processed simultaneously, as opposed to the 
serialized nature of C.

NEURAL NET ARCHITECTURE:
FEATURE VECTOR[0:19] ---> LAYER 1 - 32 NEURONS (ReLU) ---> LAYER 2 - 16 NEURONS (ReLU) ---> PREDICTION[0]

 --------------------
| ReLU(x) = max(x, 0) |
 --------------------

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

Multiplying two Q4.12 numbers returns a Q8.24 number. (RIP Kobe) Converting a Q8.24 to Q4.12 involves shifting the Q8.24 to the right by 12. This loses precision, but the number is still the same.

Comparing the respective software paths, we can estimate the Verilog-based approach of computation as being approximately 1 000 to 1 500 times faster than performing computations serially in C. This turns calculations empirically
measured to take approximately 15 seconds, to under ~20ms.

The intimate reasons why this approach is significantly faster:
1. (The Main Reason) Through Verilog, we can enlist the 87 DSP blocks on the FPGA to do multiplications in parallel. In particular, we use 64 DSP blocks to perform 64 multiplications at a time (4 neurons, 16 multiplications per neuron per clock cycle) using this accelerator,
whereas running serially on C can only perform 1 multiplication at a time.
2. (Subtle and not as impactful) Verilog has DSP blocks which contain dedicated multiplication circuits; raw combinational logic, in the first place. In base NiosV, NiosV RV32im, the processor does not have a built-in way to perform float	
multiplication (im = integer multiplication), and thus takes dozens of cycles of instructions to unpack floating point numbers and multiply them. The reason this isn't as impactful is that the NiosV processor also has an FPU component, which does have a built-in
fast float multiplier. But, critically, Verilog combines the strengths of (1) having combinational logic uniquely dedicated to multiplication and (2) deploying parallelism. Because of this, Verilog not only dwarfs the NN calculations on the
base NiosV, but also outpaces the NN calculations on the FPU-enabled NiosV (30x - 50x faster).


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
	 
	localparam neurons_per_pass = 4; // one neuron = ReLU(dot + bias) = ReLU(x0 * w0,j + x1 * w1,j + x2 * w2,j + ... + x19 * w19,j + bias_j) for the jth neuron (j is the index of the columns of the weight matrix)
	
	localparam multiplies_per_clock = 16; // 16 of (input feature * weight) per clock
													  // when 20 of these are completed (2 clock cycles as mentioned above), the bias and ReLU are added and the neuron is complete.
													  
	reg signed [15:0] input_reg [0:L0_IN-1]; // 20, 16 bit (Q4.12) numbers
	reg signed [31:0] layer0_out [0:L0_OUT-1]; // 20, 32 bit (Q8.24) numbers
	reg signed [31:0] layer1_out [0:L1_OUT-1]; // 16, 32 bit numbers
	reg signed [31:0] final_result; // one Q8.24 number
	
	// FSM states
	// S_Li: Load in the values to be processed by the ensuing combinational logic
	// S_Li_AC: On the following clock cycle, take the result.
	localparam S_IDLE = 3'd0;
	localparam S_L0 = 3'd1;
	localparam S_L0_AC = 3'd2;
	localparam S_L1 = 3'd3;
	localparam S_L1_AC = 3'd4;
	localparam S_L2 = 3'd5;
	localparam S_L2_AC = 3'd6;
	localparam S_DONE = 3'd7;
	
	
	/*
	WEIGHT FUNCTIONS
	TO BE GENERATED USING PYTHON
	*/
	
	/*
	Below: All Utility Pieces
	- Multiplier Circuit
	- Adder Circuit
	- Accumulator Array
	*/
	 
	// Multiplier Circuit
	reg signed [15:0] mul_a [0:neurons_per_pass-1][0:multiplies_per_clock]; // Inputs. Each cell of a 2D array (4x16, 4 neurons, 16 multiplies each) is a 16 bit (Q4.12) number
	reg signed [15:0] mul_b [0:neurons_per_pass-1][0:multiplies_per_clock]; // Outputs
	wire signed [31:0] mul_out [0:neurons_per_pass-1][0:multiplies_per_clock] // results, Q8.24 numbers in a 4x16 grid.
	
	// generate (not a "loop", these instructions are unrolled and all executed in parallel) block that populates an output matrix given weights and input neurons.
	// Later we will populate mul_a and mul_b with the appropriate values.
	genvar gn, gm;
	generate
		for (gn = 0; gn < neurons_per_pass; gn = gn + 1) begin : gen_neuron
			for (gm = 0; gm < multiplies_per_clock; gm = gm + 1) begin : gen_mult
				assign mul_out[gn][gm] = mul_a[gn][gm] * mul_b[gn][gm];
			end
		end
	endgenerate
	
	// Adder circuit
	// This can not all be done in one statement because you can only add numbers two at a time in Verilog
	// In each clock, we handle a chunk of a neuron (16 components on the first cycle, the remaining 4 on the following cycle with the additional 12 going unused)
	// 16 components = 16 additions. We group these into pairs and compute
	// Loop 1: 16 pairs -> 8
	// Loop 2: 8 pairs -> 4
	// Loop 3: 4 pairs -> 2
	// Finally, the ultimate pair is added.
	
	// On "even" cycles where only 4 components of a neuron are handled, the remaining 12 components are 0.
	// This algorithm is a bit inefficient for the 4-component case, but it works and is not inefficient enough for the complexity of improving it
	
	wire signed [31:0] sum_out [0:neurons_per_pass-1];
	generate
		for (gn = 0; gn < neurons_per_pass; gn = gn + 1) : gen_adder
			// 16 -> 8
			wire signed [31:0] s1 [0:7];
			for (gm = 0; gm < 8; gm = gm + 1) begin: stage1
				assign s1[gm] = mul_a[gn][2*gm] + mul_b[gn][2*gm+1];
			end
			// 8 -> 4
			wire signed [31:0] s2 [0:3];
			for (gm = 0; gm < 4; gm = gm + 1) begin: stage2
				assign s2[gm] = s1[2*gm] + s1[2*gm+1];
			end
			// 4 -> 2
			wire signed [31:0] s3 [0:1];
			for (gm = 0; gm < 2; gm = gm + 1) begin: stage3
				assign s3[gm] = s2[2*gm] + s2[2*gm+1];
			end
			// Finally, combine the ultimate pair.
			assign sum_out[gn] = s3[0] + s3[1];
		end
	endgenerate
	
	// Because the computation of a neuron is broken up across two clock cycles, we use an "accumulator" unit to merge the disparate fragments of the neuron
	reg signed [31:0] accumulator [0:neurons_per_pass-1];
	
	/*
	MAIN
	*/
	
	
	
	
	
