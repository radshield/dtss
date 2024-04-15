#ifndef DNN_H
#define DNN_H

#include <boost/predef/architecture.h>

#if BOOST_ARCH_X86_64
#include <x86intrin.h>
#endif

#define LAYER_SIZE 20000
#define CACHE_SZ 2 * 1024 * 1024

// Initialize number of layers and neurons and edges per layer
const int layer_num = 3;
const int neuron_num = LAYER_SIZE;
const int edge_num = LAYER_SIZE;
const int input_size = 10000;

// Split layer into three blocks for parallel processing
const int block_num = 3;
const int neuron_num_split = LAYER_SIZE / block_num;
const int edge_num_split = LAYER_SIZE / block_num;

inline double perceptron(double *in, double *weight, double bias, int size) {
  double sum = 0.0;

#pragma clang loop vectorize(enable) interleave(enable)
  for (int i = 0; i < size; ++i) {
    sum += in[i] * weight[i];
  }
  // Add the bias to the sum
  sum += bias;

  return sum;
}

void layer(double *input, double *output, double **weights, double *bias,
           int input_size, int output_size) {
  // Call perceptron function for each neuron
#pragma clang loop vectorize(enable)
  for (int i = 0; i < output_size; ++i) {
    double sum = output[i];
#pragma clang loop vectorize(enable) interleave(enable)
    for (int j = 0; j < input_size; ++j) {
      sum += input[j] * weights[i][j];
    }
    // Add the bias to the sum
    sum += bias[i];
    output[i] = sum;
  }
}

void clear_cache(double **input_data) {
#if BOOST_ARCH_X86_64
  for (int i = 0; i < layer_num; ++i) {
    for (int it = 0; it < neuron_num; it++) {
      _mm_clflush(&input_data[i][it]);
    }
    _mm_clflush(input_data[i]);
  }
#elif BOOST_ARCH_ARM
  long *p = new long[CACHE_SZ];

  for (int i = 0; i < CACHE_SZ; i++) {
    p[i] = rand();
  }
#endif
}

void clear_cache_weights(double **input_data) {
#if BOOST_ARCH_X86_64
  for (int it = 0; it < neuron_num; it++) {
    for (int itt = 0; itt < edge_num; itt++) {
      _mm_clflush(&input_data[it][itt]);
    }
    _mm_clflush(input_data[it]);
  }
#elif BOOST_ARCH_ARM
  long *p = new long[CACHE_SZ];

  for (int i = 0; i < CACHE_SZ; i++) {
    p[i] = rand();
  }
#endif
}

#endif
