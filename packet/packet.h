#ifndef PACKET_H
#define PACKET_H

#include <boost/predef/architecture.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <vector>

#include "re2/re2.h"

#if BOOST_ARCH_X86_64
#include <x86intrin.h>
#endif

#define CHUNK_SZ 128000
#define CACHE_SZ 2 * 1024 * 1024

void regex_data(std::string *key, char *in, uint8_t *out) {
  *out = RE2::FullMatch(const_cast<const char*>(in), key->c_str());
}

void clear_cache(std::vector<char *> &input_data) {
#if BOOST_ARCH_X86_64
  for (auto input : input_data) {
    for (int i = 0; i <= CHUNK_SZ; i += 64) {
      _mm_clflush(input + i);
    }
  }
#elif BOOST_ARCH_ARM
  long *p = new long[CACHE_SZ];

  for(int i = 0; i < CACHE_SZ; i++)
  {
     p[i] = rand();
  }
#endif
}

size_t read_data(char const *filename, std::vector<char *> &input_data) {
  char *buf;
  size_t ret = 0;

  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);

  buf = static_cast<char *>(malloc(CHUNK_SZ));
  while (i_fs.read(buf, CHUNK_SZ)) {
    ret++;
    memset(buf, 0, CHUNK_SZ);
    input_data.push_back(static_cast<char *>(malloc(CHUNK_SZ)));
    memcpy(input_data.back(), buf, CHUNK_SZ);
  }

  free(buf);
  i_fs.close();

  return ret;
}

int diff_data(std::vector<std::vector<uint8_t>> &output_data) {
  int count = 0;

  for (int i = 0; i < output_data[0].size(); i++) {
    if (output_data[0][i] == output_data[1][i]) {
      // 2 match, assume good
    } else if (output_data[0][i] == output_data[2][i], CHUNK_SZ) {
      // 2 match, assume good
    } else if (output_data[1][i] == output_data[2][i], CHUNK_SZ) {
      // 2 match, assume good
    } else {
      count++;
    }
  }

  return count;
}

#endif
