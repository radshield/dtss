#include "dnn.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <random>
#include <sched.h>
#include <thread>

const bool do_cache_clears = true;

void dtss_clear_cache(double *input, double *output) {
#if BOOST_ARCH_X86_64
  for (int i = 0; i < input_size; ++i) {
    _mm_clflush(&input[i]);
  }

  for (int i = 0; i < neuron_num_split; ++i) {
    _mm_clflush(&output[i]);
  }
#elif BOOST_ARCH_ARM
  long *p = new long[CACHE_SZ];

  for (int i = 0; i < CACHE_SZ; i++) {
    p[i] = rand();
  }
#endif
}

struct InputData {
public:
  double *input, *output, **weights, *bias;
  InputData(double *target_input, double **target_weights, double *target_bias)
      : input(target_input), weights(target_weights), bias(target_bias) {};
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

      dtss_clear_cache(input->input, input->output);

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
  std::vector<double **> outputs(3), biases(3);
  std::vector<double ***> weights(3);
  double *input;
  cpu_set_t cpuset;
  int r;

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(1000000);

  // Start the clock
  auto start = std::chrono::high_resolution_clock::now();

  std::cout << "initializing weights randomly" << std::endl;
  // Initialize weights randomly
  weights[0] = static_cast<double ***>(malloc(layer_num * sizeof(double **)));
  weights[1] = static_cast<double ***>(malloc(layer_num * sizeof(double **)));
  weights[2] = static_cast<double ***>(malloc(layer_num * sizeof(double **)));
  for (int i = 0; i < layer_num; ++i) {
    weights[0][i] =
        static_cast<double **>(malloc(neuron_num * sizeof(double *)));
    weights[1][i] =
        static_cast<double **>(malloc(neuron_num * sizeof(double *)));
    weights[2][i] =
        static_cast<double **>(malloc(neuron_num * sizeof(double *)));
    for (int j = 0; j < neuron_num; ++j) {
      weights[0][i][j] =
          static_cast<double *>(malloc(edge_num * sizeof(double)));
      weights[1][i][j] =
          static_cast<double *>(malloc(edge_num * sizeof(double)));
      weights[2][i][j] =
          static_cast<double *>(malloc(edge_num * sizeof(double)));
      for (int k = 0; k < edge_num; ++k) {
        weights[0][i][j][k] = dis(gen);
        weights[1][i][j][k] = weights[0][i][j][k];
        weights[2][i][j][k] = weights[1][i][j][k];
      }
    }
  }

  std::cout << "initializing biases randomly" << std::endl;
  // Initialize biases randomly
  biases[0] = static_cast<double **>(malloc(layer_num * sizeof(double *)));
  biases[1] = static_cast<double **>(malloc(layer_num * sizeof(double *)));
  biases[2] = static_cast<double **>(malloc(layer_num * sizeof(double *)));
  for (int i = 0; i < layer_num; ++i) {
    biases[0][i] = static_cast<double *>(malloc(neuron_num * sizeof(double)));
    biases[1][i] = static_cast<double *>(malloc(neuron_num * sizeof(double)));
    biases[2][i] = static_cast<double *>(malloc(neuron_num * sizeof(double)));
    for (int j = 0; j < neuron_num; ++j) {
      biases[0][i][j] = dis(gen);
      biases[1][i][j] = biases[0][i][j];
      biases[2][i][j] = biases[1][i][j];
    }
  }

  std::cout << "initializing outputs as 0" << std::endl;
  // Initialize outputs as 0.0
  for (int i = 0; i < outputs.size(); i++) {
    outputs[i] = static_cast<double **>(malloc(layer_num * sizeof(double *)));
    for (int it = 0; it < layer_num; ++it) {
      outputs[i][it] =
          static_cast<double *>(malloc(neuron_num * sizeof(double)));
      memset(outputs[i][it], 0, neuron_num * sizeof(double));
    }
  }

  std::cout << "initializing input vector" << std::endl;
  // Initialize input vector with an random input
  input = static_cast<double *>(malloc(input_size * sizeof(double)));
  for (int i = 0; i < input_size; ++i) {
    input[i] = dis(gen);
  }

  // Run the neural network
  std::cout << "running the neural network" << std::endl;

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

  for (int k = 0; k < num_inferences; ++k) {
    for (int it = 0; it < layer_num; ++it) {
      // Run 3 times
      for (int i = 0; i < 3; i++) {
        // Multithread each block
        for (int cur_block = 0; cur_block < block_num; ++cur_block) {
          int start_idx = cur_block * neuron_num_split;

          auto input_data = new InputData(
              it == 0 ? cur_input : cur_input + sizeof(double) * start_idx,
              weights[i][it], biases[i][it] + sizeof(double) * start_idx);

          if (cur_block == 0) {
            switch (i) {
            case 0:
              jobqueue_0.push(input_data);
              input_data->output = outputs[0][it] + sizeof(double) * start_idx;
              break;
            case 1:
              jobqueue_1.push(input_data);
              input_data->output = outputs[1][it] + sizeof(double) * start_idx;
              break;
            case 2:
              jobqueue_2.push(input_data);
              input_data->output = outputs[2][it] + sizeof(double) * start_idx;
              break;
            }
          } else if (cur_block == 1) {
            switch (i) {
            case 0:
              jobqueue_1.push(input_data);
              input_data->output = outputs[1][it] + sizeof(double) * start_idx;
              break;
            case 1:
              jobqueue_2.push(input_data);
              input_data->output = outputs[2][it] + sizeof(double) * start_idx;
              break;
            case 2:
              jobqueue_0.push(input_data);
              input_data->output = outputs[0][it] + sizeof(double) * start_idx;
              break;
            }
          } else if (cur_block == 2) {
            switch (i) {
            case 0:
              jobqueue_2.push(input_data);
              input_data->output = outputs[2][it] + sizeof(double) * start_idx;
              break;
            case 1:
              jobqueue_0.push(input_data);
              input_data->output = outputs[0][it] + sizeof(double) * start_idx;
              break;
            case 2:
              jobqueue_1.push(input_data);
              input_data->output = outputs[1][it] + sizeof(double) * start_idx;
              break;
            }
          }
        }

        // Wait for threads to finish
        while (
            !(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
          continue;
      }

      cur_input = outputs[0][it - 1 < 0 ? 0 : it - 1];
    }

    std::cout << "Run " << k << " complete" << std::endl;
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
    free(outputs[0][i]);
    free(outputs[1][i]);
    free(outputs[2][i]);
  }
  for (int i = 0; i < 3; i++) {
    free(outputs[i]);
    free(weights[i]);
    free(biases[i]);
  }
  free(input);

  return 0;
}
