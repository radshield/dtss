#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <openssl/evp.h>
#include <ostream>
#include <unistd.h>
#include <vector>
#include <x86intrin.h>

void encrypt_data(uint8_t *key, char const *filename, size_t in_index,
                  uint8_t *out) {
  uint8_t iv[16] = {0};
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int outlen1, outlen2;
  uint8_t *in = (uint8_t *)malloc(1024);
  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);

  i_fs.seekg(in_index * 1024);
  i_fs.read((char *)in, 1024);
  i_fs.close();

  EVP_EncryptInit(ctx, EVP_aes_256_ecb(), key, iv);
  EVP_EncryptUpdate(ctx, out, &outlen1, in, 1024);
  EVP_EncryptFinal(ctx, out + outlen1, &outlen2);

  free(in);
}

void clear_cache() {
  int fd;
  std::string data = "3";

  sync();
  fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
  write(fd, data.c_str(), sizeof(char));
  close(fd);
}

void read_data(char const *filename, std::vector<uint8_t *> &input_data) {
  char *buf;

  std::ifstream i_fs(filename, std::ios::in | std::ios::binary);

  buf = (char *)malloc(1024);
  while (i_fs.read(buf, 1024)) {
    memset(buf, 0, 1024);
    input_data.push_back((uint8_t *)malloc(1024));
    memcpy(input_data.back(), buf, 1024);
  }

  free(buf);
  i_fs.close();
}

int diff_data(std::vector<std::vector<uint8_t *>> &output_data) {
  int count = 0;

  for (int i = 0; i < output_data[0].size(); i++) {
    if (memcmp(output_data[0][i], output_data[1][i], 1024) == 0) {
      // 2 match, assume good
    } else if (memcmp(output_data[0][i], output_data[2][i], 1024) == 0) {
      // 2 match, assume good
    } else if (memcmp(output_data[1][i], output_data[2][i], 1024) == 0) {
      // 2 match, assume good
    } else {
      count++;
    }
  }

  return count;
}

int main(int argc, char const *argv[]) {
  long long tmp_count;
  uint8_t key[32] = ".rPUkt=4;4*2c1Mk6Zk9L0p09)MA=3k";
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
      output_data[i].push_back((uint8_t *)malloc(1040));
    malloc_end[i] = std::chrono::steady_clock::now();

    encrypt_begin[i] = std::chrono::steady_clock::now();
    for (int it = 0; it < output_data[0].size(); it++) {
      encrypt_data(key, argv[1], it, output_data[i][it]);
    }
    encrypt_end[i] = std::chrono::steady_clock::now();

    if (i != 2) {
      cache_begin[i] = std::chrono::steady_clock::now();
      clear_cache();
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
