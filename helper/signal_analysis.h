/*
signal_analysis.c

This file contains the functions we use to extract relevant data from the signal
and subsequently construct the feature vector. There are five core features common to
each level of implementation:

1. Log Energy
2. Zero-Crossing Rate (ZCR)
3. Spectral Centroid
4. Spectral Flatness
5. Spectral Bandwidth

(Note: All function headers prefixed by "spectral" are analyzed using the FFT of a frame)
The following seven features are unique to our level 0 implementation (where we distinguish between tones, noises and speech):

6. Peak Amplitude
7. Crest Factor
8. Dominant Frequency
9. Spectral Rolloff
10. Spectral Entropy
11. Low-Band Power Ratio
12. High-Band Power Ratio
*/

#ifndef SIGNAL_ANALYSIS_H
#define SIGNAL_ANALYSIS_H

#define RECORDING_LENGTH   40000      // total samples
#define FRAME_LENGTH       256        // samples per frame
#define HOP_SIZE           128        // overlap step
#define SAMPLING_RATE      8000       // Hz
#define FEATURES_0         12         // 12 features in the level 0 feature vector

#define STANDARD_GRAPH_HEIGHT   120
#define STANDARD_GRAPH_WIDTH    270

// Derived constants
#define FRAMES_PER_RECORDING (((RECORDING_LENGTH - FRAME_LENGTH) / HOP_SIZE) + 1)

// For real-valued signals: bins 0..N/2 inclusive
#define NO_FREQ_BINS       ((FRAME_LENGTH / 2) + 1)

// Frequency resolution (Hz per bin)
#define BIN_SPACING        ((double)SAMPLING_RATE / FRAME_LENGTH)

// Sign function for integers
// Returns the sign of an integer
int sign_int(int n);

// Arithmetic Mean
double arithmetic_mean(double frame[FRAME_LENGTH]);

// Geometric Mean
double geometric_mean(double frame[FRAME_LENGTH]);

// Log Energy
// Measures overall signal power on a log scale, acts as a proxy for loudness and signal strength
// Separates silence vs active signal
double log_energy(int frame[FRAME_LENGTH]);

// Zero-Crossing Rate
// Measures how often the signal crosses zero, measures high frequency content --> proxy for noise
double zero_crossing_rate(int frame[FRAME_LENGTH]);

// Spectral Centroid
// Center of mass of the spectrum, measures the spectrum's brightness
double spectral_centroid(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);

// Spectral Flatness
// How flat the spectrum is. Tone has low flatness, noise has high flatness
double spectral_flatness(double frame_fft[NO_FREQ_BINS]);

// Spectral Bandwidth
// Spread of frequencies around the centroid, measures how wide the spectrum is.
// Tones have narrow bandwidth, noise and speech have higher bandwidth
double spectral_bandwidth(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);

/*
LEVEL 1 EXCLUSIVE FEATURES
*/

// Peak Amplitude
// Maximum absolute value of signal
double peak_amplitude(int frame[FRAME_LENGTH]);

// Crest Factor
// Peak relative to RMS
// Reveals information about the shape of the waveform
double crest_factor(int frame[FRAME_LENGTH]);

// Dominant Frequency
// Frequency with the maximum energy
double dominant_frequency(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);

// Spectral Rolloff
// Frequency below which 85% of the energy lies
// Measures energy distribution cutoff, tones have low rolloff, noise has high rolloff
double spectral_rolloff(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);

// Spectral Entropy
// Normalized entropy of the spectrum, measures disorder
// Noise has high entropy, tones have low entropy
double spectral_entropy(double frame_fft[NO_FREQ_BINS]);

// Low-Band Power Ratio
// Energy in low frequencies vs in total
double low_band_power_ratio(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);

// High-Band Power Ratio
// Measures high frequency content
double high_band_power_ratio(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]);

#endif