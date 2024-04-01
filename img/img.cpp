#include "img.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <vector>
#include <x86intrin.h>

#define REDUNANCY_NUM 3

int main(int argc, char const *argv[]) {
  long long tmp_count;
  std::chrono::steady_clock::time_point begin, end;
  std::vector<std::chrono::steady_clock::time_point> read_begin(REDUNANCY_NUM),
      read_end(REDUNANCY_NUM), malloc_begin(REDUNANCY_NUM),
      malloc_end(REDUNANCY_NUM), encrypt_begin, encrypt_end, cache_begin,
      cache_end;

  std::vector<std::vector<std::vector<int>>> output_data(3);

  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " IMG MATCH" << std::endl;
    return -1;
  }

  std::string filename_img = argv[1];
  std::string filename_match = argv[2];

  begin = std::chrono::steady_clock::now();

  for (int i = 0; i < REDUNANCY_NUM; i++) {
    read_begin[i] = std::chrono::steady_clock::now();
    auto img = cv::imread(filename_img, cv::IMREAD_COLOR);
    auto match = cv::imread(filename_match, cv::IMREAD_COLOR);
    read_end[i] = std::chrono::steady_clock::now();

    malloc_begin[i] = std::chrono::steady_clock::now();
    for (int j = 0; j < img.rows - match.rows + 1; j++) {
      output_data[i].push_back(std::vector<int>());
      for (int k = 0; k < img.cols - match.cols + 1; k++) {
        output_data[i][j].push_back(0);
      }
    }
    malloc_end[i] = std::chrono::steady_clock::now();

    for (int j = 0; j < img.rows; j++) {
      for (int k = 0; k < img.cols; k++) {
        encrypt_begin.push_back(std::chrono::steady_clock::now());
        output_data[i][j][k] = nccscore_data(&img, &match, j, k);
        encrypt_end.push_back(std::chrono::steady_clock::now());
      }
    }

    // Clear cache
    if (i != REDUNANCY_NUM - 1) {
      cache_begin.push_back(std::chrono::steady_clock::now());
      clear_cache(&img);
      clear_cache(&match);
      cache_end.push_back(std::chrono::steady_clock::now());
    }
  }

  int count = diff_data(output_data);

  end = std::chrono::steady_clock::now();

  std::cout << count << " / " << output_data[0].size() << std::endl
            << std::endl;

  std::cout << "Total runtime: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     begin)
                   .count()
            << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < read_begin.size(); i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     read_end[i] - read_begin[i])
                     .count();
  }
  std::cout << "Disk read runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < malloc_begin.size(); i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     malloc_end[i] - malloc_begin[i])
                     .count();
  }
  std::cout << "Malloc runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < encrypt_begin.size(); i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     encrypt_end[i] - encrypt_begin[i])
                     .count();
  }
  std::cout << "Encrypt runtime: " << tmp_count << " us" << std::endl;

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
