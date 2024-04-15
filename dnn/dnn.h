#ifndef DNN_H
#define DNN_H

#include <boost/predef/architecture.h>
#include <vector>

#if BOOST_ARCH_X86_64
#include <x86intrin.h>
#endif

#define LAYER_SIZE 20000
#define CACHE_SZ 2 * 1024 * 1024

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

void clear_cache(std::vector<double *> &input_data) {
#if BOOST_ARCH_X86_64
  for (auto input : input_data) {
    for (int i = 0; i <= CHUNK_SZ; i += 64) {
      _mm_clflush(input + i);
    }
  }
#elif BOOST_ARCH_ARM
  long *p = new long[CACHE_SZ];

  for(int i = 0; i < CACHE_SZ; i++)
  {
     p[i] = rand();
  }
#endif
}

#endif
