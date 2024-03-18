#ifndef ZIP_H
#define ZIP_H

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <x86intrin.h>
#include <zlib.h>

#define CHUNK_SZ 128000 + 32000

void compress_data(uint8_t *in, uint8_t *prev, uint8_t *out) {
  z_stream z_str;

  z_str.zalloc = Z_NULL;
  z_str.zfree = Z_NULL;
  z_str.opaque = Z_NULL;
  z_str.avail_in = 0;
  z_str.next_in = Z_NULL;

  z_str.avail_out = 128 * 1000;
  z_str.next_out = out;
  deflateInit(&z_str, Z_DEFAULT_COMPRESSION);

  if (prev != nullptr) {
    z_str.avail_in = 32000;
    z_str.next_in = prev + CHUNK_SZ - 32000;

    deflate(&z_str, Z_SYNC_FLUSH);
  }

  z_str.avail_in = 128000;
  z_str.next_in = in;

  deflate(&z_str, Z_SYNC_FLUSH);

  deflateEnd(&z_str);
}

void compress_data_disk(char const *filename, off_t in_index, uint8_t *in_buf,
                        off_t prev_index, uint8_t *prev_buf, uint8_t *out_buf) {
  z_stream z_str;

  z_str.zalloc = Z_NULL;
  z_str.zfree = Z_NULL;
  z_str.opaque = Z_NULL;
  z_str.avail_in = 0;
  z_str.next_in = Z_NULL;

  z_str.avail_out = 128 * 1000;
  z_str.next_out = out_buf;

  int fd = open(filename, O_RDONLY | O_DIRECT);

  deflateInit(&z_str, Z_DEFAULT_COMPRESSION);

  if (prev_buf != nullptr) {
    lseek(fd, prev_index * CHUNK_SZ, SEEK_SET);
    read(fd, (char *)prev_buf, CHUNK_SZ);

    z_str.avail_in = 32000;
    z_str.next_in = prev_buf + CHUNK_SZ - 32000;

    deflate(&z_str, Z_SYNC_FLUSH);
  }

  lseek(fd, in_index * CHUNK_SZ, SEEK_SET);
  read(fd, (char *)in_buf, CHUNK_SZ);

  z_str.avail_in = 128000;
  z_str.next_in = in_buf;

  deflate(&z_str, Z_SYNC_FLUSH);

  deflateEnd(&z_str);

  close(fd);
}

void clear_cache(std::vector<uint8_t *> &input_data) {
  for (auto input : input_data) {
    memset(input, 0, 128000);
    for (int i = 0; i <= 128000; i += 64) {
      _mm_clflush(input + i);
    }
  }
}

void read_data(char const *filename, std::vector<uint8_t *> &input_data) {
  char *buf;

  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);

  buf = static_cast<char *>(malloc(128000));
  while (i_fs.read(buf, 128000)) {
    memset(buf, 0, 128000);
    input_data.push_back((uint8_t *)malloc(128000));
    memcpy(input_data.back(), buf, 128000);
  }

  free(buf);
  i_fs.close();
}

int diff_data(std::vector<std::vector<uint8_t *>> &output_data) {
  int count = 0;

  for (int i = 0; i < output_data[0].size(); i++) {
    if (memcmp(output_data[0][i], output_data[1][i], 128000) == 0) {
      // 2 match, assume good
    } else if (memcmp(output_data[0][i], output_data[2][i], 128000) == 0) {
      // 2 match, assume good
    } else if (memcmp(output_data[1][i], output_data[2][i], 128000) == 0) {
      // 2 match, assume good
    } else {
      count++;
    }
  }

  return count;
}

#endif
