#include "dnn.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <random>
#include <sched.h>
#include <thread>

// Initialize number of layers and neurons and edges per layer
const int layer_num = 3;
const int neuron_num = LAYER_SIZE;
const int edge_num = LAYER_SIZE;
const int input_size = 10000;

// Split layer into three blocks for parallel processing
const int block_num = 3;
const int neuron_num_split = LAYER_SIZE / block_num;
const int edge_num_split = LAYER_SIZE / block_num;

struct InputData {
public:
  double *input, *output, **weights, *bias;
};

std::atomic_bool jobs_done = false;

void worker_process(boost::lockfree::spsc_queue<InputData *> *job_queue) {
  while (!job_queue->empty() || !jobs_done) {
    if (job_queue->empty())
      continue;
    else {
      // Process is in job queue, remove from queue and process
      InputData *input = job_queue->front();

      layer(input->input, input->output, input->weights, input->bias,
            input_size, neuron_num_split);

      job_queue->pop();
      delete input;
    }
  }
}

int main() {
  // Initialize random number generator
  std::random_device rd;
  // Initialize time keeper
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(-1.0, 1.0);

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(1000000);

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

  // Run the neural network
  // Start the clock
  auto start = std::chrono::high_resolution_clock::now();

  // Start threads
  std::thread tmr_0(worker_process, &jobqueue_0);
  std::thread tmr_1(worker_process, &jobqueue_1);
  std::thread tmr_2(worker_process, &jobqueue_2);

  CPU_ZERO(&cpuset);
  CPU_SET(1, &cpuset);
  r = pthread_setaffinity_np(tmr_0.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 0 to core" << std::endl;

  CPU_ZERO(&cpuset);
  CPU_SET(2, &cpuset);
  r = pthread_setaffinity_np(tmr_1.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 1 to core" << std::endl;

  CPU_ZERO(&cpuset);
  CPU_SET(3, &cpuset);
  r = pthread_setaffinity_np(tmr_2.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 2 to core" << std::endl;

  double *cur_input = input;
  // loop through layers
  for (int i = 0; i < layer_num; ++i) {
    for (int cur_block = 0; cur_block < block_num; ++cur_block) {
      int start_idx = cur_block * neuron_num_split;

      layer(cur_input + sizeof(double) * start_idx,
            outputs[i] + sizeof(double) * start_idx,
            weights[i] + sizeof(double) * start_idx,
            biases[i] + sizeof(double) * start_idx, input_size,
            neuron_num_split);
    }
    cur_input = outputs[i - 1];
  }

  // stop the clock
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
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
