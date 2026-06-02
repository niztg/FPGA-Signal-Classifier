# FPGA Speaker Authentication System

A real-time speaker authentication pipeline implemented on the Intel DE1-SoC FPGA. The system captures audio from the board's microphone peripheral, extracts time-domain and frequency-domain features including MFCC coefficients, and runs inference through a pre-trained multi-layer perceptron to classify whether the speaker is authorized or unauthorized.

---

## How It Works

### Signal Acquisition and Chunking
The system continuously captures audio from the DE1-SoC microphone peripheral. Each recording is spliced into disjointed 0.5-second chunks, which are processed independently. The final authorization verdict uses a **majority vote** across all chunks — the class that receives the most individual votes determines the overall result.

### Feature Extraction
Each 0.5-second chunk is characterized by a 20-dimensional feature vector:
- 4 base time-domain features
- 16 **MFCC (Mel-Frequency Cepstral Coefficient)** properties derived from the chunk's Fourier transform

MFCCs capture the spectral envelope of the audio signal in a perceptually-motivated frequency space, making them robust features for speaker identification.

### Neural Network Inference
Classification is performed by a **multi-layer perceptron (MLP)** with the following architecture:

```
Input (20 features) → Hidden Layer 1 (36 neurons) → Hidden Layer 2 (16 neurons) → Output (2 classes)
```

The network was trained externally in Python using ~100 recordings per class (authorized / unauthorized). After training, weights were extracted into a C header file and integrated into the embedded firmware running on the DE1-SoC's soft-core processor.

### Hardware/Software Co-Design
The system runs on the DE1-SoC's embedded soft-core processor, handling FFT and MFCC feature extraction, MLP inference, and GUI rendering entirely in C firmware.

The architecture was designed with hardware acceleration in mind — the Verilog infrastructure for offloading DSP computation to dedicated acceleration blocks is in place, but the accelerator is not yet fully implemented. Completing this would allow the compute-intensive FFT and feature extraction stages to run in hardware, significantly improving throughput over the current software-only approach.

| Layer | Implementation |
|---|---|
| FFT and feature extraction | C firmware (soft-core processor) |
| MLP inference and control flow | C firmware (soft-core processor) |
| GUI rendering and user interaction | C firmware via VGA output |
| Hardware DSP acceleration | Infrastructure present, not yet implemented |

---

## GUI and User Interface

The system provides a real-time visualization interface over VGA with the following views:

- **Time domain graph** — raw captured audio waveform
- **Frequency domain graph** — FFT magnitude spectrum with dial-controlled scale and pan
- **Spectrogram** — frequency heatmap over time
- **MFCC radar plot** — per-chunk feature vector visualized as a radar chart

Physical dials on the DE1-SoC allow users to:
- Scale and pan the frequency domain graph to inspect specific spectral regions
- Scroll through chunk-by-chunk MFCC and feature vector data across the recording

Two independent channels allow simultaneous storage and comparison of two recordings, each with configurable recording length.

---

## Tech Stack

| Component | Technology |
|---|---|
| FPGA Platform | Intel DE1-SoC |
| Hardware Description | Verilog |
| Firmware | C |
| HDL Simulation | ModelSim |
| Synthesis | Quartus Prime |
| Model Training | Python (NumPy, scikit-learn) |
| Signal Processing | FFT, MFCC extraction |

---

## Repository Structure

```
/
├── hardware/          # Verilog modules (FFT acceleration, VGA controller, peripherals)
├── firmware/          # C source for soft-core processor (inference engine, GUI, control)
├── model/             # Python training scripts and exported weight files
└── docs/              # Block diagrams and system documentation
```

---

## System Requirements

- Intel DE1-SoC development board
- Quartus Prime (for synthesis and programming)
- ModelSim (for HDL simulation)
- Python 3.x with NumPy and scikit-learn (for retraining the model)
