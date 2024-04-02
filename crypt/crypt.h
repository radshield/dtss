#ifndef CRYPT_H
#define CRYPT_H

#include <boost/predef/architecture.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <openssl/evp.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <vector>

#if BOOST_ARCH_X86_64
#include <x86intrin.h>
#endif

#define CHUNK_SZ 8192
#define CACHE_SZ 2 * 1024 * 1024

void encrypt_data(uint8_t *key, uint8_t *in, uint8_t *out) {
  uint8_t iv[128] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                     0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int outlen1, outlen2;

  EVP_EncryptInit(ctx, EVP_aes_256_ecb(), key, iv);
  EVP_EncryptUpdate(ctx, out, &outlen1, in, CHUNK_SZ);
  EVP_EncryptFinal(ctx, out + outlen1, &outlen2);
}

void encrypt_data_disk(uint8_t *key, std::string in_file, size_t in_index,
                       std::string out_file) {
  uint8_t iv[128] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                     0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int outlen1, outlen2;
  uint8_t *in_buf = static_cast<uint8_t *>(malloc(CHUNK_SZ)),
          *out_buf = static_cast<uint8_t *>(malloc(CHUNK_SZ + 16));

  int fd = open(in_file.c_str(), O_RDONLY | O_DIRECT);
  lseek(fd, in_index * CHUNK_SZ, SEEK_SET);
  read(fd, in_buf, CHUNK_SZ);
  close(fd);

  EVP_EncryptInit(ctx, EVP_aes_256_ecb(), key, iv);
  EVP_EncryptUpdate(ctx, out_buf, &outlen1, in_buf, CHUNK_SZ);
  EVP_EncryptFinal(ctx, out_buf + outlen1, &outlen2);

  fd = open(out_file.c_str(), O_WRONLY | O_DIRECT);
  lseek(fd, in_index * CHUNK_SZ, SEEK_SET);
  write(fd, out_buf, CHUNK_SZ);
  fsync(fd);
  close(fd);
}

void clear_cache(std::vector<uint8_t *> &input_data) {
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

void clear_cache_disk() {
  int fd;
  std::string data = "3";

  sync();
  fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
  write(fd, data.c_str(), sizeof(char));
  fsync(fd);
  close(fd);
}

size_t read_data(char const *filename, std::vector<uint8_t *> &input_data) {
  char *buf;
  size_t ret = 0;

  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);

  buf = static_cast<char *>(malloc(CHUNK_SZ));
  while (i_fs.read(buf, CHUNK_SZ)) {
    ret++;
    memset(buf, 0, CHUNK_SZ);
    input_data.push_back(static_cast<uint8_t *>(malloc(CHUNK_SZ)));
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

  buf = static_cast<char *>(malloc(CHUNK_SZ + 16));
  while (i_fs.read(buf, CHUNK_SZ)) {
    ret++;
    memset(buf, 0, CHUNK_SZ + 16);
    o_fs.write(buf, CHUNK_SZ + 16);
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
