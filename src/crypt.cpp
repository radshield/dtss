#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <openssl/evp.h>
#include <thread>
#include <vector>

void encrypt_data(uint8_t *key, uint8_t *in, uint8_t *out) {
  uint8_t iv[16] = {0};
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int outlen1, outlen2;

  EVP_EncryptInit(ctx, EVP_aes_256_ecb(), key, iv);
  EVP_EncryptUpdate(ctx, out, &outlen1, in, 1024);
  EVP_EncryptFinal(ctx, out + outlen1, &outlen2);
}

int main(int argc, char const *argv[]) {
  char *buf;
  uint8_t key[32] = ".rPUkt=4;4*2c1Mk6Zk9L0p09)MA=3k";

  std::vector<uint8_t *> input_data;
  std::vector<uint8_t *> output_data[3];

  std::ifstream i_fs(argv[1], std::ios::in | std::ios::binary);

  buf = (char *)malloc(1024);
  while (i_fs.read(buf, 1024)) {
    memset(buf, 0, 1024);
    input_data.push_back((uint8_t *)malloc(1024));
    memcpy(input_data.back(), buf, 1024);
  }
  free(buf);
  i_fs.close();

  for (int i = 0; i < input_data.size() - input_data.size() % 3; i++) {
    output_data[0].push_back((uint8_t *)malloc(1040));
    output_data[1].push_back((uint8_t *)malloc(1040));
    output_data[2].push_back((uint8_t *)malloc(1040));
  }

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();

  for (int i = 0; i < 3; i++) {
    std::ifstream ifs(argv[1], std::ios::in | std::ios::binary);

    buf = (char *)malloc(1024);
    while (ifs.read(buf, 1024)) {
      memset(buf, 0, 1024);
      input_data.push_back((uint8_t *)malloc(1024));
      memcpy(input_data.back(), buf, 1024);
    }
    free(buf);

    ifs.close();

    for (int it = 0; it < output_data[0].size(); it++) {
      encrypt_data(key, input_data[it], output_data[i][it]);
    }
  }

  // Compare data
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

  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

  std::cout << "Runtime: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     begin)
                   .count()
            << " µs" << std::endl;
  std::cout << count << " / " << output_data[0].size() << std::endl;

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
