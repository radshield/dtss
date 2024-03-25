#include "crypt.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <openssl/evp.h>
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

  std::vector<uint8_t *> input_data;
  std::vector<std::vector<uint8_t *>> output_data(3);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  begin = std::chrono::steady_clock::now();

  for (int i = 0; i < 3; i++) {
    read_begin[i] = std::chrono::steady_clock::now();
    read_data(argv[1], input_data);
    read_end[i] = std::chrono::steady_clock::now();

    malloc_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < input_data.size() - input_data.size() % 3; it++)
      output_data[i].push_back((uint8_t *)malloc(CHUNK_SZ + 16));
    malloc_end[i] = std::chrono::steady_clock::now();

    encrypt_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < output_data[0].size(); it++) {
      encrypt_data(key, input_data[it], output_data[i][it]);
    }
    encrypt_end[i] = std::chrono::steady_clock::now();

    if (i != 2) {
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
