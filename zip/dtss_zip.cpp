#include "zip.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <openssl/evp.h>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

#if BOOST_ARCH_X86_64
#include <x86intrin.h>
#endif

const bool do_cache_clears = true;

void clear_cache(uint8_t *in, uint8_t *prev) {
#if BOOST_ARCH_X86_64
  if (in != nullptr)
    for (int i = 0; i <= CHUNK_SZ; i += 64)
      _mm_clflush(in + i);

  if (prev != nullptr)
    for (int i = CHUNK_SZ - 32000; i <= CHUNK_SZ; i += 64)
      _mm_clflush(prev + i);
#endif
}

struct InputData {
public:
  uint8_t *in, *prev, *out;
};

std::atomic_bool jobs_done = false;

void worker_process(boost::lockfree::spsc_queue<InputData *> *job_queue) {
  while (!job_queue->empty() || !jobs_done) {
    if (job_queue->empty())
      continue;
    else {
      // Process is in job queue, remove from queue and process
      InputData *input = job_queue->front();
      compress_data(input->in, input->prev, input->out);
      if (do_cache_clears)
        clear_cache(input->in, input->prev);
      job_queue->pop();
      delete input;
    }
  }
}

int main(int argc, char const *argv[]) {
  long long tmp_count;
  cpu_set_t cpuset;
  int r;
  std::chrono::steady_clock::time_point begin, end, read_begin, read_end,
      malloc_begin, malloc_end;
  std::vector<std::chrono::steady_clock::time_point> compress_begin(6),
      compress_end(6), cache_begin(5), cache_end(5);

  std::vector<uint8_t *> input_data;
  std::vector<std::vector<uint8_t *>> output_data(3);

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(1000000);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  begin = std::chrono::steady_clock::now();

  read_begin = std::chrono::steady_clock::now();
  read_data(argv[1], input_data);
  read_end = std::chrono::steady_clock::now();

  malloc_begin = std::chrono::steady_clock::now();
  for (int i = 0; i < input_data.size() - input_data.size() % 3; i++) {
    output_data[0].push_back((uint8_t *)malloc(128000));
    output_data[1].push_back((uint8_t *)malloc(128000));
    output_data[2].push_back((uint8_t *)malloc(128000));
  }
  malloc_end = std::chrono::steady_clock::now();

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

  compress_begin[0] = std::chrono::steady_clock::now();

  // Distribute #1
  for (int i = 0; i < output_data[0].size(); i += 6) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_0->prev = (i != 0) ? input_data[i - 1] : nullptr;
    in_0->in = input_data[i];
    in_0->out = output_data[0][i];

    in_1->prev = input_data[i + 1];
    in_1->in = input_data[i + 2];
    in_1->out = output_data[1][i + 2];

    in_2->prev = input_data[i + 3];
    in_2->in = input_data[i + 4];
    in_2->out = output_data[2][i + 4];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  compress_end[0] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[0] = std::chrono::steady_clock::now();
#if BOOST_ARCH_ARM
  clear_cache(input_data);
#endif
  cache_end[0] = std::chrono::steady_clock::now();

  compress_begin[1] = std::chrono::steady_clock::now();

  // Distribute #2
  for (int i = 0; i < output_data[0].size(); i += 6) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_0->prev = input_data[i];
    in_0->in = input_data[i + 1];
    in_0->out = output_data[0][i + 1];

    in_1->prev = input_data[i + 2];
    in_1->in = input_data[i + 3];
    in_1->out = output_data[1][i + 3];

    in_2->prev = input_data[i + 4];
    in_2->in = input_data[i + 5];
    in_2->out = output_data[2][i + 5];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  compress_end[1] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[1] = std::chrono::steady_clock::now();
#if BOOST_ARCH_ARM
  clear_cache(input_data);
#endif
  cache_end[1] = std::chrono::steady_clock::now();

  compress_begin[2] = std::chrono::steady_clock::now();

  // Distribute #3
  for (int i = 0; i < output_data[0].size(); i += 3) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_1->prev = (i != 0) ? input_data[i - 1] : nullptr;
    in_1->in = input_data[i];
    in_1->out = output_data[0][i];

    in_2->prev = input_data[i + 1];
    in_2->in = input_data[i + 2];
    in_2->out = output_data[1][i + 2];

    in_0->prev = input_data[i + 3];
    in_0->in = input_data[i + 4];
    in_0->out = output_data[2][i + 4];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  compress_end[2] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[2] = std::chrono::steady_clock::now();
#if BOOST_ARCH_ARM
  clear_cache(input_data);
#endif
  cache_end[2] = std::chrono::steady_clock::now();

  compress_begin[3] = std::chrono::steady_clock::now();

  // Distribute #4
  for (int i = 0; i < output_data[0].size(); i += 3) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_1->prev = input_data[i];
    in_1->in = input_data[i + 1];
    in_1->out = output_data[0][i + 1];

    in_2->prev = input_data[i + 2];
    in_2->in = input_data[i + 3];
    in_2->out = output_data[1][i + 3];

    in_0->prev = input_data[i + 4];
    in_0->in = input_data[i + 5];
    in_0->out = output_data[2][i + 5];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  compress_end[3] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[3] = std::chrono::steady_clock::now();
#if BOOST_ARCH_ARM
  clear_cache(input_data);
#endif
  cache_end[3] = std::chrono::steady_clock::now();

  compress_begin[4] = std::chrono::steady_clock::now();

  // Distribute #5
  for (int i = 0; i < output_data[0].size(); i += 6) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_2->prev = (i != 0) ? input_data[i - 1] : nullptr;
    in_2->in = input_data[i];
    in_2->out = output_data[0][i];

    in_0->prev = input_data[i + 1];
    in_0->in = input_data[i + 2];
    in_0->out = output_data[1][i + 2];

    in_1->prev = input_data[i + 3];
    in_1->in = input_data[i + 4];
    in_1->out = output_data[2][i + 4];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  compress_end[4] = std::chrono::steady_clock::now();

  // Clear cache
  cache_begin[4] = std::chrono::steady_clock::now();
#if BOOST_ARCH_ARM
  clear_cache(input_data);
#endif
  cache_end[4] = std::chrono::steady_clock::now();

  compress_begin[5] = std::chrono::steady_clock::now();

  // Distribute #6
  for (int i = 0; i < output_data[0].size(); i += 6) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_2->prev = input_data[i];
    in_2->in = input_data[i + 1];
    in_2->out = output_data[0][i + 1];

    in_0->prev = input_data[i + 2];
    in_0->in = input_data[i + 3];
    in_0->out = output_data[1][i + 3];

    in_1->prev = input_data[i + 4];
    in_1->in = input_data[i + 5];
    in_1->out = output_data[2][i + 5];

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

  compress_end[5] = std::chrono::steady_clock::now();

  end = std::chrono::steady_clock::now();

  // Compare data
  int count = diff_data(output_data);

  std::cout << count << " / " << output_data[0].size() << std::endl
            << std::endl;

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

  std::cout << "Malloc runtime: "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   malloc_end - malloc_begin)
                   .count()
            << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < compress_begin.size(); i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     compress_end[i] - compress_begin[i])
                     .count();
  }
  std::cout << "Compress runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < cache_begin.size(); i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     cache_end[i] - cache_begin[i])
                     .count();
  }
  std::cout << "Cache clear runtime: " << tmp_count << " us" << std::endl
            << std::endl;

  // Cleanup data
  for (int i = 0; i < output_data[0].size() % 3; i++) {
    free(output_data[0][i]);
    free(output_data[1][i]);
    free(output_data[2][i]);
  }

  for (uint8_t *input : input_data)
    free(input);

  return 0;
}
