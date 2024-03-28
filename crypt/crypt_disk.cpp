#include "crypt.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <sys/fcntl.h>
#include <unistd.h>
#include <vector>
#include <x86intrin.h>

#define REDUNANCY_NUM 3

int main(int argc, char const *argv[]) {
  long long tmp_count;
  size_t input_size;
  uint8_t key[256] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                      0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
                      0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33,
                      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};
  std::chrono::steady_clock::time_point begin, end;
  std::vector<std::chrono::steady_clock::time_point> read_begin(REDUNANCY_NUM), read_end(REDUNANCY_NUM),
      encrypt_begin(REDUNANCY_NUM), encrypt_end(REDUNANCY_NUM), cache_begin(REDUNANCY_NUM - 1), cache_end(REDUNANCY_NUM - 1);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  std::string filename = argv[1];

  begin = std::chrono::steady_clock::now();

  for (int i = 0; i < REDUNANCY_NUM; i++) {
    read_begin[i] = std::chrono::steady_clock::now();
    input_size = read_data_disk(filename.c_str(), filename + ".out.0");
    read_data_disk(filename.c_str(), filename + ".out.1");
    read_data_disk(filename.c_str(), filename + ".out.2");
    read_end[i] = std::chrono::steady_clock::now();

    input_size = input_size - input_size % 3;

    encrypt_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < input_size; it++) {
      encrypt_data_disk(key, filename, it,
                        filename + ".out." + std::to_string(i));
    }
    encrypt_end[i] = std::chrono::steady_clock::now();

    if (i != REDUNANCY_NUM - 1) {
      cache_begin[i] = std::chrono::steady_clock::now();
      clear_cache_disk();
      cache_end[i] = std::chrono::steady_clock::now();
    }
  }

  end = std::chrono::steady_clock::now();

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
