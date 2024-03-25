#include "crypt.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>
#include <x86intrin.h>

struct InputData {
public:
  uint8_t *key;
  size_t in_index;
  std::string filename;
  InputData(std::string file) : filename(file) {};
};

std::atomic_bool jobs_done = false;

void worker_process(int w_num,
                    boost::lockfree::spsc_queue<InputData *> *job_queue) {
  while (!job_queue->empty() || !jobs_done) {
    if (job_queue->empty())
      continue;
    else {
      // Process is in job queue, remove from queue and process
      InputData *input = job_queue->front();
      encrypt_data_disk(input->key, input->filename, input->in_index,
                        input->filename + ".out." + std::to_string(w_num));
      job_queue->pop();
      delete input;
    }
  }
}

int main(int argc, char const *argv[]) {
  long long tmp_count;
  size_t input_size;
  uint8_t key[256] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                      0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                      0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33,
                      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};
  cpu_set_t cpuset;
  int r;
  std::chrono::steady_clock::time_point begin, end, read_begin, read_end;
  std::vector<std::chrono::steady_clock::time_point> encrypt_begin(3),
      encrypt_end(3), cache_begin(2), cache_end(2);

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(10000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(10000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(10000000);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  std::string filename = argv[1];

  begin = std::chrono::steady_clock::now();

  read_begin = std::chrono::steady_clock::now();
  input_size = read_data_disk(filename.c_str(), filename + ".out.0");
  read_data_disk(filename.c_str(), filename + ".out.1");
  read_data_disk(filename.c_str(), filename + ".out.2");
  read_end = std::chrono::steady_clock::now();

  input_size = input_size - input_size % 3;

  // Start threads
  std::thread tmr_0(worker_process, 0, &jobqueue_0);
  std::thread tmr_1(worker_process, 1, &jobqueue_1);
  std::thread tmr_2(worker_process, 2, &jobqueue_2);

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

  encrypt_begin[0] = std::chrono::steady_clock::now();

  // Distribute #1
  for (int i = 0; i < input_size; i += 3) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_0->key = key;
    in_0->in_index = i;
    in_0->filename = argv[1];

    in_1->key = key;
    in_1->in_index = i + 1;
    in_1->filename = argv[1];

    in_2->key = key;
    in_2->in_index = i + 2;
    in_2->filename = argv[1];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  encrypt_end[0] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[0] = std::chrono::steady_clock::now();
  clear_cache_disk();
  cache_end[0] = std::chrono::steady_clock::now();

  encrypt_begin[1] = std::chrono::steady_clock::now();

  // Distribute #2
  for (int i = 0; i < input_size; i += 3) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_0->key = key;
    in_0->in_index = i + 1;
    in_0->filename = argv[1];

    in_1->key = key;
    in_1->in_index = i + 2;
    in_1->filename = argv[1];

    in_2->key = key;
    in_2->in_index = i;
    in_2->filename = argv[1];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  encrypt_end[1] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[1] = std::chrono::steady_clock::now();
  clear_cache_disk();
  cache_end[1] = std::chrono::steady_clock::now();

  encrypt_begin[2] = std::chrono::steady_clock::now();

  // Distribute #3
  for (int i = 0; i < input_size; i += 3) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_0->key = key;
    in_0->in_index = i + 2;
    in_0->filename = argv[1];

    in_1->key = key;
    in_1->in_index = i;
    in_1->filename = argv[1];

    in_2->key = key;
    in_2->in_index = i + 1;
    in_2->filename = argv[1];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // All jobs pushed, send signal to end after compute done
  jobs_done = true;

  // Wait for threads to end
  tmr_0.join();
  tmr_1.join();
  tmr_2.join();

  encrypt_end[2] = std::chrono::steady_clock::now();

  end = std::chrono::steady_clock::now();

  std::cout << "Total runtime: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     begin)
                   .count()
            << " us" << std::endl;

  std::cout << "Disk read runtime: "
            << std::chrono::duration_cast<std::chrono::microseconds>(read_end -
                                                                     read_begin)
                   .count()
            << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < 3; i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     encrypt_end[i] - encrypt_begin[i])
                     .count();
  }
  std::cout << "Encrypt runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < 2; i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     cache_end[i] - cache_begin[i])
                     .count();
  }
  std::cout << "Cache clear runtime: " << tmp_count << " us" << std::endl
            << std::endl;

  return 0;
}
