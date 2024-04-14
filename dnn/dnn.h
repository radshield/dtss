#ifndef DNN_H
#define DNN_H

#define LAYER_SIZE 20000
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
#pragma clang loop vectorize(enable) interleave(enable)
  for (int i = 0; i < output_size; ++i) {
    double sum = 0.0;
    for (int j = 0; j < input_size; ++j) {
      sum += input[j] * weights[i][j];
    }
    // Add the bias to the sum
    sum += bias[i];
    output[i] = sum;
  }
}

#endif
