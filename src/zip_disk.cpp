#include "zip.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

int main(int argc, char const *argv[]) {
  long long tmp_count;
  size_t input_size;
  std::chrono::steady_clock::time_point begin, end;
  std::vector<std::chrono::steady_clock::time_point> read_begin(3), read_end(3),
      compress_begin(3), compress_end(3), cache_begin(3), cache_end(3);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  std::string filename = argv[1];

  begin = std::chrono::steady_clock::now();

  for (int i = 0; i < 3; i++) {
    read_begin[i] = std::chrono::steady_clock::now();
    input_size = read_data_disk(filename.c_str(), filename + ".out.0");
    read_data_disk(filename.c_str(), filename + ".out.1");
    read_data_disk(filename.c_str(), filename + ".out.2");
    read_end[i] = std::chrono::steady_clock::now();

    compress_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < input_size; it++) {
      if (it != 0)
        compress_data_disk(filename, it, it - 1,
                           filename + ".out." + std::to_string(i));
      else
        compress_data_disk(filename, it, -1,
                           filename + ".out." + std::to_string(i));
    }
    compress_end[i] = std::chrono::steady_clock::now();

    if (i != 2) {
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
  for (int i = 0; i < 3; i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     read_end[i] - read_begin[i])
                     .count();
  }
  std::cout << "Disk read runtime: " << tmp_count << " us" << std::endl;

  tmp_count = 0;
  for (int i = 0; i < 3; i++) {
    tmp_count += std::chrono::duration_cast<std::chrono::microseconds>(
                     compress_end[i] - compress_begin[i])
                     .count();
  }
  std::cout << "Compress runtime: " << tmp_count << " us" << std::endl;

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
