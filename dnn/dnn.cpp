#include "dnn.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>

int main() {
  // Initialize random number generator
  std::random_device rd;
  // Initialize time keeper
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(-1.0, 1.0);

  // Initialize number of layers and neurons and edges per layer
  const int layer_num = 3;
  const int neuron_num = LAYER_SIZE;
  const int edge_num = LAYER_SIZE;
  const int input_size = 10000;

  std::cout << "initializing weights randomly" << std::endl;
  // Initialize weights randomly
  double ***weights = (double ***)malloc(layer_num * sizeof(double **));
  for (int i = 0; i < layer_num; ++i) {
    weights[i] = (double **)malloc(neuron_num * sizeof(double *));
    for (int j = 0; j < neuron_num; ++j) {
      weights[i][j] = (double *)malloc(edge_num * sizeof(double));
      for (int k = 0; k < edge_num; ++k) {
        weights[i][j][k] = dis(gen);
      }
    }
  }
  std::cout << "initializing biases randomly" << std::endl;
  // Initialize biases randomly
  double **biases = (double **)malloc(layer_num * sizeof(double *));
  for (int i = 0; i < layer_num; ++i) {
    biases[i] = (double *)malloc(neuron_num * sizeof(double));
    for (int j = 0; j < neuron_num; ++j) {
      biases[i][j] = dis(gen);
    }
  }

  std::cout << "initializing outputs as 0" << std::endl;
  // Initialize outputs as 0.0
  double **outputs = (double **)malloc(layer_num * sizeof(double *));
  for (int i = 0; i < layer_num; ++i) {
    outputs[i] = (double *)malloc(neuron_num * sizeof(double));
    for (int j = 0; j < neuron_num; ++j) {
      outputs[i][j] = 0.0;
    }
  };

  std::cout << "initializing input vector" << std::endl;
  // Initialize input vector with an random input
  double *input = (double *)malloc(input_size * sizeof(double));
  for (int i = 0; i < input_size; ++i) {
    input[i] = dis(gen);
  }

  std::cout << "running the neural network" << std::endl;
  // Run the neural network
  // Start the clock
  auto start = std::chrono::high_resolution_clock::now();

  // loop through layers
  for (int it = 0; it < 3; it++) {
    for (int k = 0; k < 40; ++k) {
      for (int i = 0; i < layer_num; ++i) {
        if (i == 0) {
          layer(input, outputs[i], weights[i], biases[i], input_size, neuron_num);
        } else {
          layer(outputs[i - 1], outputs[i], weights[i], biases[i], input_size,
                neuron_num);
        }
      }
    }
  }

  // stop the clock
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;

  // print total time taken to run the neural network
  std::cout << "Time taken: " << elapsed_seconds.count() << " seconds"
            << std::endl;

  for (int i = 0; i < layer_num; ++i) {
    for (int j = 0; j < neuron_num; ++j) {
      free(weights[i][j]);
    }
    free(weights[i]);
    free(biases[i]);
    free(outputs[i]);
  }
  free(weights);
  free(biases);
  free(outputs);
  free(input);

  return 0;
}
