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
The following seven features are unique to our level 1 implementation (where we distinguish between tones, noises and speech):

6. Peak Amplitude
7. Crest Factor
8. Dominant Frequency
9. Spectral Rolloff
10. Spectral Entropy
11. Low-Band Power Ratio
12. High-Band Power Ratio
*/

#define FRAME_LENGTH       256
#define NO_FREQ_BINS       128
#define SAMPLING_RATE      8000
#define EPSILON            1e-12

#include "signal_analysis.h"


// Sign function for integers
// Returns the sign of an integer
int sign_int(int n){
    if (n > 0) return 1;
    else if (n < 0) return -1;
    else return 0;
}

// Arithmetic Mean
double arithmetic_mean(double frame[NO_FREQ_BINS]){
    double sum = 0.0;
    for (int i = 0; i < NO_FREQ_BINS; i++){
        sum += frame[i];
    }

    return (double) sum / NO_FREQ_BINS;
}

// Geometric Mean
double geometric_mean(double frame[NO_FREQ_BINS]){
    double log_sum = 0.0; // Use a log sum to prevent overflow
    for (int i = 0; i < NO_FREQ_BINS; i++){
        log_sum += log(frame[i] + EPSILON); // want small offset incase argument value is 0 otherwise
    }

    return (double) exp(log_sum / NO_FREQ_BINS);
}

// Log Energy
// Measures overall signal power on a log scale, acts as a proxy for loudness and signal strength
// Separates silence vs active signal
double log_energy(int frame[FRAME_LENGTH]){
    double running_sum = 0.0;
    for (int n = 0; n < FRAME_LENGTH; n++){
        running_sum += (frame[n] * frame[n]);
    }
    return (double) log(running_sum + EPSILON);
}

// Zero-Crossing Rate
// Measures how often the signal crosses zero, measures high frequency content --> proxy for noise
double zero_crossing_rate(int frame[FRAME_LENGTH]){
    int running_sum = 0;
    for (int n = 1; n < FRAME_LENGTH; n++){
        running_sum += abs(
            sign_int(frame[n]) - sign_int(frame[n-1])
        );
    }

    return (double) running_sum / (2*FRAME_LENGTH);
}

/*
Note: What are the frequency bins? 

The FFT is indexed by integers k
The frequency bins convert each integer k to roughly the linear frequency that they correspond to
The frequency bin array is of size FRAME_LENGTH / 2, as |FFT| is even for real signals.
*/

// Spectral Centroid
// Center of mass of the spectrum, measures the spectrum's brightness
double spectral_centroid(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    double sum_top = 0.0;
    double sum_bottom = 0.0;

    for (int k = 0; k < NO_FREQ_BINS; k++){
        sum_top += frequency_bins[k] * frame_fft[k];
        sum_bottom += frame_fft[k];
    }
    
    return (double) sum_top / (sum_bottom + EPSILON); // epsilon to guard against divide by 0
}

// Spectral Flatness
// How flat the spectrum is. Tone has low flatness, noise has high flatness
double spectral_flatness(double frame_fft[NO_FREQ_BINS]){
    return (double) geometric_mean(frame_fft) / (arithmetic_mean(frame_fft) + EPSILON);
}

// Spectral Bandwidth
// Spread of frequencies around the centroid, measures how wide the spectrum is.
// Tones have narrow bandwidth, noise and speech have higher bandwidth
double spectral_bandwidth(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    double sum_top = 0.0;
    double sum_bottom = 0.0;

    double centroid = spectral_centroid(frame_fft, frequency_bins);

    for (int k = 0; k < NO_FREQ_BINS; k++){
        double inner_term = (frequency_bins[k] - centroid);
        sum_top += (inner_term * inner_term) * frame_fft[k];
        sum_bottom += frame_fft[k];
    }

    double quotient = (double) sum_top / (sum_bottom + EPSILON);
    return sqrt(quotient);
}

/*
LEVEL 1 EXCLUSIVE FEATURES
*/

// Peak Amplitude
// Maximum absolute value of signal
double peak_amplitude(int frame[FRAME_LENGTH]){
    int max_value = abs(frame[0]);
    for (int i = 1; i < FRAME_LENGTH; i++){
        if (abs(frame[i]) > max_value){
            max_value = abs(frame[i]);
        }
    }
    return (double) max_value;
}

// Crest Factor
// Peak relative to RMS
// Reveals information about the shape of the waveform
double crest_factor(int frame[FRAME_LENGTH]){
    double sum_of_squares = 0.0;
    for (int i = 0; i < FRAME_LENGTH; i++){
        sum_of_squares += (frame[i] * frame[i]);
    }
    return peak_amplitude(frame) / (sqrt(sum_of_squares / FRAME_LENGTH) + EPSILON);
}

// Dominant Frequency
// Frequency with the maximum energy
double dominant_frequency(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    int dominant_k = 0;
    double max_value = frame_fft[0];
    for (int k = 1; k < NO_FREQ_BINS; k++){
        if (frame_fft[k] > max_value){
            dominant_k = k;
            max_value = frame_fft[k];
        }
    }

    return frequency_bins[dominant_k];
}

// Spectral Rolloff
// Frequency below which 85% of the energy lies
// Measures energy distribution cutoff, tones have low rolloff, noise has high rolloff
double spectral_rolloff(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    double sum_fft = 0.0;
    double running_sum = 0.0;

    for (int k = 0; k < NO_FREQ_BINS; k++){
        sum_fft += frame_fft[k];
    }
    
    int final_index = 0;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        running_sum += frame_fft[l];
        if (running_sum >= (0.85 * sum_fft)){
            final_index = l;
            break;
        }
    }

    return frequency_bins[final_index];
}

// Spectral Entropy
// Normalized entropy of the spectrum, measures disorder
// Noise has high entropy, tones have low entropy
double spectral_entropy(double frame_fft[NO_FREQ_BINS]){
        double sum_fft = 0.0;
        double entropy = 0.0;
        for (int k = 0; k < NO_FREQ_BINS; k++){
            sum_fft += frame_fft[k];
        }    

    for (int l = 0; l < NO_FREQ_BINS; l++){
        double p_l = (double) frame_fft[l] / (sum_fft + EPSILON);
        entropy += p_l * (log(p_l + EPSILON));
    }

    return -entropy;
}

// Low-Band Power Ratio
// Energy in low frequencies vs in total
double low_band_power_ratio(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    double f_L = 1000.0;
    // iterate through the frequency bins to find which index we need to stop at
    int stopping_index = 0;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        if (frequency_bins[l] >= f_L){
            stopping_index = l;
            break;
        }
    }

    double sum_fft = 0.0;
    double running_sum = 0.0;
    for (int m = 0; m < NO_FREQ_BINS; m++){
        sum_fft += frame_fft[m];
    }    

    for (int k = 0; k < stopping_index; k++){
        running_sum += frame_fft[k];
    }

    return (double) running_sum / (sum_fft + EPSILON);
}

// High-Band Power Ratio
// Measures high frequency content
double high_band_power_ratio(double frame_fft[NO_FREQ_BINS], double frequency_bins[NO_FREQ_BINS]){
    double f_H = 2000.0;
    // iterate through the frequency bins to find which index we need to start at
    int starting_index = 0;
    for (int l = 0; l < NO_FREQ_BINS; l++){
        if (frequency_bins[l] >= f_H){
            starting_index = l;
            break;
        }
    }

    double sum_fft = 0.0;
    double running_sum = 0.0;
    for (int m = 0; m < NO_FREQ_BINS; m++){
        sum_fft += frame_fft[m];
    }    

    for (int k = starting_index; k < NO_FREQ_BINS; k++){
        running_sum += frame_fft[k];
    }

    return (double) running_sum / (sum_fft + EPSILON);
}