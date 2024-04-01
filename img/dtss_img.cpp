#include "img.h"

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>
#include <x86intrin.h>

struct InputData {
public:
  cv::Mat *img, *match;
  int row, col;
  int *output_data;
  InputData(cv::Mat *target_img, cv::Mat *match_img)
      : img(target_img), match(match_img){};
};

std::atomic_bool jobs_done = false;

void worker_process(boost::lockfree::spsc_queue<InputData *> *job_queue) {
  while (!job_queue->empty() || !jobs_done) {
    if (job_queue->empty())
      continue;
    else {
      // Process is in job queue, remove from queue and process
      InputData *input = job_queue->front();
      *(input->output_data) =
          nccscore_data(input->img, input->match, input->row, input->col);
      job_queue->pop();
      delete input;
    }
  }
}

int main(int argc, char const *argv[]) {
  long long tmp_count;
  uint8_t key[256] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                      0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                      0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33,
                      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};
  cpu_set_t cpuset;
  int r;
  std::chrono::steady_clock::time_point begin, end, read_begin, read_end,
      malloc_begin, malloc_end;
  std::vector<std::vector<std::chrono::steady_clock::time_point>> img_begin(3),
      img_end(3), cache_begin(2), cache_end(2);

  std::vector<std::vector<std::vector<int>>> output_data(3);

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(1000000);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(1000000);

  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " IMG MATCH" << std::endl;
    return -1;
  }

  std::string filename_img = argv[1];
  std::string filename_match = argv[2];

  begin = std::chrono::steady_clock::now();

  read_begin = std::chrono::steady_clock::now();
  auto img = cv::imread(filename_img, cv::IMREAD_COLOR);
  auto match = cv::imread(filename_match, cv::IMREAD_COLOR);
  read_end = std::chrono::steady_clock::now();

  malloc_begin = std::chrono::steady_clock::now();
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < img.rows - match.rows + 1; j++) {
      output_data[i].push_back(std::vector<int>());
      for (int k = 0; k < img.cols - match.cols + 1; k++) {
        output_data[i][j].push_back(0);
      }
    }
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

  int row_blocks = img.rows / match.rows;
  int col_blocks = img.cols / match.cols;

  // Offset of first block to match with
  for (int i = 0; i < match.rows; i++) {
    for (int it = 0; it < match.cols; it++) {
      img_begin[i].push_back(std::chrono::steady_clock::now());

      for (int j = i; j < row_blocks * col_blocks; j += 3) {
        auto in_0 = new InputData(&img, &match);
        auto in_1 = new InputData(&img, &match);
        auto in_2 = new InputData(&img, &match);

        in_0->row = (j % row_blocks) * row_blocks;
        in_0->col = (j / row_blocks) * col_blocks;
        in_0->output_data = &output_data.at(i).at(in_0->row).at(in_0->col);

        in_1->row = ((j + 1) % row_blocks) * row_blocks;
        in_1->col = ((j + 1) / row_blocks) * col_blocks;
        in_1->output_data = &output_data.at(i).at(in_1->row).at(in_1->col);

        in_2->row = ((j + 2) % row_blocks) * row_blocks;
        in_2->col = ((j + 2) / row_blocks) * col_blocks;
        in_2->output_data = &output_data.at(i).at(in_2->row).at(in_2->col);

        jobqueue_0.push(in_0);
        jobqueue_1.push(in_1);
        jobqueue_2.push(in_2);
      }

      // Wait for compute to end
      while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
        continue;

      img_end[i].push_back(std::chrono::steady_clock::now());

      // Clear cache
      cache_begin[i].push_back(std::chrono::steady_clock::now());
      clear_cache(&img);
      clear_cache(&match);
      cache_end[i].push_back(std::chrono::steady_clock::now());

      img_begin[i].push_back(std::chrono::steady_clock::now());

      for (int j = i; j < row_blocks * col_blocks; j += 3) {
        auto in_0 = new InputData(&img, &match);
        auto in_1 = new InputData(&img, &match);
        auto in_2 = new InputData(&img, &match);

        in_2->row = (j % row_blocks) * row_blocks;
        in_2->col = (j / row_blocks) * col_blocks;
        in_2->output_data = &output_data.at(i).at(in_2->row).at(in_2->col);

        in_0->row = ((j + 1) % row_blocks) * row_blocks;
        in_0->col = ((j + 1) / row_blocks) * col_blocks;
        in_0->output_data = &output_data.at(i).at(in_0->row).at(in_0->col);

        in_1->row = ((j + 2) % row_blocks) * row_blocks;
        in_1->col = ((j + 2) / row_blocks) * col_blocks;
        in_1->output_data = &output_data.at(i).at(in_1->row).at(in_1->col);

        jobqueue_0.push(in_0);
        jobqueue_1.push(in_1);
        jobqueue_2.push(in_2);
      }

      // Wait for compute to end
      while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
        continue;

      img_end[i].push_back(std::chrono::steady_clock::now());

      // Clear cache
      cache_begin[i].push_back(std::chrono::steady_clock::now());
      clear_cache(&img);
      clear_cache(&match);
      cache_end[i].push_back(std::chrono::steady_clock::now());

      img_begin[i].push_back(std::chrono::steady_clock::now());

      for (int j = i; j < row_blocks * col_blocks; j += 3) {
        auto in_0 = new InputData(&img, &match);
        auto in_1 = new InputData(&img, &match);
        auto in_2 = new InputData(&img, &match);

        in_1->row = (j % row_blocks) * row_blocks;
        in_1->col = (j / row_blocks) * col_blocks;
        in_1->output_data = &output_data.at(i).at(in_1->row).at(in_1->col);

        in_2->row = ((j + 1) % row_blocks) * row_blocks;
        in_2->col = ((j + 1) / row_blocks) * col_blocks;
        in_2->output_data = &output_data.at(i).at(in_2->row).at(in_2->col);

        in_0->row = ((j + 2) % row_blocks) * row_blocks;
        in_0->col = ((j + 2) / row_blocks) * col_blocks;
        in_0->output_data = &output_data.at(i).at(in_0->row).at(in_0->col);

        jobqueue_0.push(in_0);
        jobqueue_1.push(in_1);
        jobqueue_2.push(in_2);
      }

      // Wait for compute to end
      while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
        continue;

      img_end[i].push_back(std::chrono::steady_clock::now());

      // Clear cache
      cache_begin[i].push_back(std::chrono::steady_clock::now());
      clear_cache(&img);
      clear_cache(&match);
      cache_end[i].push_back(std::chrono::steady_clock::now());
    }
  }

  // All jobs pushed, send signal to end after compute done
  jobs_done = true;

  // Wait for threads to end
  tmr_0.join();
  tmr_1.join();
  tmr_2.join();

  // Compare data
  int count = diff_data(output_data);

  end = std::chrono::steady_clock::now();

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
  for (int i = 0; i < 3; i++) {
    for (int it = 0; it < img_begin[i].size(); it++) {
      tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                       img_end[i][it] - img_begin[i][it])
                       .count();
    }
  }
  std::cout << "Encrypt runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < 2; i++) {
    for (int it = 0; it < cache_begin[i].size(); it++) {
      tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                       cache_end[i][it] - cache_begin[i][it])
                       .count();
    }
  }
  std::cout << "Cache clear runtime: " << tmp_count << " us" << std::endl
            << std::endl;

  return 0;
}
