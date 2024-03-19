#ifndef ZIP_H
#define ZIP_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <x86intrin.h>
#include <zlib.h>

#define CHUNK_SZ 128000

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

  z_str.avail_in = CHUNK_SZ;
  z_str.next_in = in;

  deflate(&z_str, Z_SYNC_FLUSH);

  deflateEnd(&z_str);
}

void compress_data_disk(std::string filename, off_t in_index, off_t prev_index,
                        std::string out_filename) {
  z_stream z_str;
  uint8_t *in_buf = static_cast<uint8_t *>(aligned_alloc(512, CHUNK_SZ)),
          *out_buf = static_cast<uint8_t *>(aligned_alloc(512, CHUNK_SZ)),
          *prev_buf = static_cast<uint8_t *>(aligned_alloc(512, CHUNK_SZ));

  z_str.zalloc = Z_NULL;
  z_str.zfree = Z_NULL;
  z_str.opaque = Z_NULL;
  z_str.avail_in = 0;
  z_str.next_in = Z_NULL;

  z_str.avail_out = CHUNK_SZ;
  z_str.next_out = out_buf;

  deflateInit(&z_str, Z_DEFAULT_COMPRESSION);

  int input_fd = open(filename.c_str(), O_RDONLY | O_DIRECT);

  if (prev_buf != nullptr) {
    lseek(input_fd, prev_index * CHUNK_SZ, SEEK_SET);
    read(input_fd, (char *)prev_buf, CHUNK_SZ);
  }

  lseek(input_fd, in_index * CHUNK_SZ, SEEK_SET);
  read(input_fd, (char *)in_buf, CHUNK_SZ);

  close(input_fd);

  if (prev_buf != nullptr) {
    z_str.avail_in = 32000;
    z_str.next_in = prev_buf + CHUNK_SZ - 32000;

    deflate(&z_str, Z_SYNC_FLUSH);
  }

  z_str.avail_in = CHUNK_SZ;
  z_str.next_in = in_buf;

  deflate(&z_str, Z_SYNC_FLUSH);

  deflateEnd(&z_str);

  std::ofstream o_fs(out_filename, std::ios::out | std::ios::binary);
  o_fs.seekp(in_index * CHUNK_SZ);
  o_fs << out_buf;

  free(in_buf);
  free(out_buf);
  free(prev_buf);
}

void clear_cache(std::vector<uint8_t *> &input_data) {
  for (auto input : input_data) {
    for (int i = 0; i <= CHUNK_SZ; i += 64) {
      _mm_clflush(input + i);
    }
  }
}

void clear_cache_disk() {
  int fd;
  std::string data = "3";

  sync();
  fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
  write(fd, data.c_str(), sizeof(char));
  close(fd);
}

size_t read_data(char const *filename, std::vector<uint8_t *> &input_data) {
  char *buf;
  size_t ret = 0;

  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);

  buf = static_cast<char *>(aligned_alloc(512, CHUNK_SZ));
  while (i_fs.read(buf, CHUNK_SZ)) {
    ret++;
    memset(buf, 0, CHUNK_SZ);
    input_data.push_back(static_cast<uint8_t *>(aligned_alloc(512, CHUNK_SZ)));
    memcpy(input_data.back(), buf, CHUNK_SZ);
  }

  free(buf);
  i_fs.close();

  return ret;
}

size_t read_data_disk(std::string filename, std::string out_filename) {
  char *buf;
  size_t ret = 0;

  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);
  std::ofstream o_fs(out_filename, std::ios::out | std::ios::binary);

  buf = static_cast<char *>(aligned_alloc(512, CHUNK_SZ));
  while (i_fs.read(buf, CHUNK_SZ)) {
    ret++;
    memset(buf, 0, CHUNK_SZ);
    o_fs << buf;
  }

  o_fs.close();
  i_fs.close();
  free(buf);

  return ret;
}

int diff_data(std::vector<std::vector<uint8_t *>> &output_data) {
  int count = 0;

  for (int i = 0; i < output_data[0].size(); i++) {
    if (memcmp(output_data[0][i], output_data[1][i], CHUNK_SZ) == 0) {
      // 2 match, assume good
    } else if (memcmp(output_data[0][i], output_data[2][i], CHUNK_SZ) == 0) {
      // 2 match, assume good
    } else if (memcmp(output_data[1][i], output_data[2][i], CHUNK_SZ) == 0) {
      // 2 match, assume good
    } else {
      count++;
    }
  }

  return count;
}

#endif
