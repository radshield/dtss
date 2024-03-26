#include "img.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <vector>
#include <x86intrin.h>

int main(int argc, char const *argv[]) {
  long long tmp_count;
  uint8_t key[256] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                      0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                      0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33,
                      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};
  std::chrono::steady_clock::time_point begin, end;
  std::vector<std::chrono::steady_clock::time_point> read_begin(3), read_end(3),
      malloc_begin(3), malloc_end(3), encrypt_begin(3), encrypt_end(3),
      cache_begin(3), cache_end(3);

  std::vector<std::vector<std::vector<int>>> output_data(3);

  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " IMG MATCH" << std::endl;
    return -1;
  }

  std::string filename_img = argv[1];
  std::string filename_match = argv[2];

  begin = std::chrono::steady_clock::now();

  for (int i = 0; i < 3; i++) {
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

    encrypt_begin[i] = std::chrono::steady_clock::now();
    for (int j = 0; j < img.rows - match.rows; j++)
      for (int k = 0; k < img.cols - match.cols + 1; k++)
        output_data[i][j][k] = nccscore_data(&img, &match, j, k);
    encrypt_end[i] = std::chrono::steady_clock::now();

    if (i != 2) {
      cache_begin[i] = std::chrono::steady_clock::now();
      clear_cache(&img);
      clear_cache(&match);
      cache_end[i] = std::chrono::steady_clock::now();
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
  for (int i = 0; i < 3; i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     read_end[i] - read_begin[i])
                     .count();
  }
  std::cout << "Disk read runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < 3; i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     malloc_end[i] - malloc_begin[i])
                     .count();
  }
  std::cout << "Malloc runtime: " << tmp_count << " us" << std::endl;

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
