#include "zip.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <openssl/evp.h>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

struct InputData {
public:
  off_t in_index, prev_index;
  std::string filename;
  InputData(std::string file) : filename(file){};
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
      compress_data_disk(input->filename, input->in_index, input->prev_index,
                         input->filename + ".out." + std::to_string(w_num));
      job_queue->pop();
      delete input;
    }
  }
}

int main(int argc, char const *argv[]) {
  long long tmp_count;
  cpu_set_t cpuset;
  int r;
  std::chrono::steady_clock::time_point begin, end, read_begin, read_end;
  std::vector<std::chrono::steady_clock::time_point> compress_begin(6),
      compress_end(6), cache_begin(5), cache_end(5);

  size_t input_size;

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(1000000);

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

  compress_begin[0] = std::chrono::steady_clock::now();

  // Distribute #1
  for (int i = 0; i < input_size; i += 6) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_0->prev_index = (i != 0) ? i - 1 : -1;
    in_0->in_index = i;

    in_1->prev_index = i + 1;
    in_1->in_index = i + 2;

    in_2->prev_index = i + 3;
    in_2->in_index = i + 4;

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
  clear_cache_disk();
  cache_end[0] = std::chrono::steady_clock::now();

  compress_begin[1] = std::chrono::steady_clock::now();

  // Distribute #2
  for (int i = 0; i < input_size; i += 6) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_0->prev_index = i;
    in_0->in_index = i + 1;

    in_1->prev_index = i + 2;
    in_1->in_index = i + 3;

    in_2->prev_index = i + 4;
    in_2->in_index = i + 5;

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
  clear_cache_disk();
  cache_end[1] = std::chrono::steady_clock::now();

  compress_begin[2] = std::chrono::steady_clock::now();

  // Distribute #3
  for (int i = 0; i < input_size; i += 3) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_1->prev_index = (i != 0) ? i - 1 : -1;
    in_1->in_index = i;

    in_2->prev_index = i + 1;
    in_2->in_index = i + 2;

    in_0->prev_index = i + 3;
    in_0->in_index = i + 4;

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
  clear_cache_disk();
  cache_end[2] = std::chrono::steady_clock::now();

  compress_begin[3] = std::chrono::steady_clock::now();

  // Distribute #4
  for (int i = 0; i < input_size; i += 3) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_1->prev_index = i;
    in_1->in_index = i + 1;

    in_2->prev_index = i + 2;
    in_2->in_index = i + 3;

    in_0->prev_index = i + 4;
    in_0->in_index = i + 5;

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
  clear_cache_disk();
  cache_end[3] = std::chrono::steady_clock::now();

  compress_begin[4] = std::chrono::steady_clock::now();

  // Distribute #5
  for (int i = 0; i < input_size; i += 6) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_2->prev_index = (i != 0) ? i - 1 : -1;
    in_2->in_index = i;

    in_0->prev_index = i + 1;
    in_0->in_index = i + 2;

    in_1->prev_index = i + 3;
    in_1->in_index = i + 4;

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
  clear_cache_disk();
  cache_end[4] = std::chrono::steady_clock::now();

  compress_begin[5] = std::chrono::steady_clock::now();

  // Distribute #6
  for (int i = 0; i < input_size; i += 6) {
    auto in_0 = new InputData(filename);
    auto in_1 = new InputData(filename);
    auto in_2 = new InputData(filename);

    in_2->prev_index = i;
    in_2->in_index = i + 1;

    in_0->prev_index = i + 2;
    in_0->in_index = i + 3;

    in_1->prev_index = i + 4;
    in_1->in_index = i + 5;

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

  return 0;
}
