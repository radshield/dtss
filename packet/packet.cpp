#include "packet.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <openssl/evp.h>
#include <ostream>
#include <vector>
#include <x86intrin.h>

#define REDUNANCY_NUM 3

int main(int argc, char const *argv[]) {
  long long tmp_count;
  std::string key = "^(?:1-)?((?:R|RO|Ro)?[:|.]?\\s?\\d{3}[-|.]?\\d{4}[-|/]F\\d{2}-\\d{2})$";
  std::chrono::steady_clock::time_point begin, end;
  std::vector<std::chrono::steady_clock::time_point> read_begin(REDUNANCY_NUM),
      read_end(REDUNANCY_NUM), malloc_begin(REDUNANCY_NUM),
      malloc_end(REDUNANCY_NUM), encrypt_begin(REDUNANCY_NUM),
      encrypt_end(REDUNANCY_NUM), cache_begin(REDUNANCY_NUM - 1),
      cache_end(REDUNANCY_NUM - 1);

  std::vector<char *> input_data;
  std::vector<std::vector<uint8_t>> output_data(3);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  begin = std::chrono::steady_clock::now();

  for (int i = 0; i < REDUNANCY_NUM; i++) {
    read_begin[i] = std::chrono::steady_clock::now();
    read_data(argv[1], input_data);
    read_end[i] = std::chrono::steady_clock::now();

    malloc_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < input_data.size() - input_data.size() % 3; it++)
      output_data[i].push_back(false);
    malloc_end[i] = std::chrono::steady_clock::now();

    encrypt_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < output_data[0].size(); it++) {
      regex_data(&key, input_data[it], &output_data[i][it]);
    }
    encrypt_end[i] = std::chrono::steady_clock::now();

    if (i != REDUNANCY_NUM - 1) {
      cache_begin[i] = std::chrono::steady_clock::now();
      clear_cache(input_data);
      cache_end[i] = std::chrono::steady_clock::now();
    }
  }

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

  for (char *input : input_data)
    free(input);

  return 0;
}
