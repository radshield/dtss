#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <openssl/evp.h>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>
#include <x86intrin.h>

struct InputData {
public:
  uint8_t *key, *iv;
  uint8_t *in, *out;
};

std::atomic_bool jobs_done = false;

void encrypt_data(InputData &input) {
  uint8_t iv[16] = {0};
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int outlen1, outlen2;

  EVP_EncryptInit(ctx, EVP_aes_256_ecb(), input.key, iv);
  EVP_EncryptUpdate(ctx, input.out, &outlen1, input.in, 1024);
  EVP_EncryptFinal(ctx, input.out + outlen1, &outlen2);
}

void worker_process(boost::lockfree::spsc_queue<InputData *> *job_queue) {
  while (!job_queue->empty() || !jobs_done) {
    if (job_queue->empty())
      continue;
    else {
      // Process is in job queue, remove from queue and process
      InputData *input = job_queue->front();
      job_queue->pop();
      encrypt_data(*input);
      delete input;
    }
  }
}

void clear_cache(std::vector<uint8_t *> &input_data) {
  for (auto input : input_data) {
    for (int i = 0; i <= 1024; i += 64) {
      _mm_clflush(input + i);
    }
  }
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
  uint8_t key[32] = ".rPUkt=4;4*2c1Mk6Zk9L0p09)MA=3k";
  cpu_set_t cpuset;
  int r;

  std::vector<uint8_t *> input_data;
  std::vector<std::vector<uint8_t *>> output_data(3);

  boost::lockfree::spsc_queue<InputData *> jobqueue_0(128);
  boost::lockfree::spsc_queue<InputData *> jobqueue_1(128);
  boost::lockfree::spsc_queue<InputData *> jobqueue_2(128);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " FILENAME" << std::endl;
    return -1;
  }

  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();

  read_data(argv[1], input_data);

  for (int i = 0; i < input_data.size() - input_data.size() % 3; i++) {
    output_data[0].push_back((uint8_t *)malloc(1040));
    output_data[1].push_back((uint8_t *)malloc(1040));
    output_data[2].push_back((uint8_t *)malloc(1040));
  }

  // Start threads
  std::thread tmr_0(worker_process, &jobqueue_0);
  std::thread tmr_1(worker_process, &jobqueue_1);
  std::thread tmr_2(worker_process, &jobqueue_2);

  CPU_ZERO(&cpuset);
  CPU_SET(1, &cpuset);
  r = pthread_setaffinity_np(tmr_0.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 0 to core" << std::endl;

  CPU_ZERO(&cpuset);
  CPU_SET(2, &cpuset);
  r = pthread_setaffinity_np(tmr_1.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 1 to core" << std::endl;

  CPU_ZERO(&cpuset);
  CPU_SET(3, &cpuset);
  r = pthread_setaffinity_np(tmr_2.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 2 to core" << std::endl;

  // Distribute #1
  for (int i = 0; i < output_data[0].size(); i += 3) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_0->key = key;
    in_0->in = input_data[i];
    in_0->out = output_data[0][i];

    in_1->key = key;
    in_1->in = input_data[i + 1];
    in_1->out = output_data[1][i + 1];

    in_2->key = key;
    in_2->in = input_data[i + 2];
    in_2->out = output_data[2][i + 2];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  // Clear cache
  clear_cache(input_data);

  // Distribute #2
  for (int i = 0; i < output_data[0].size(); i += 3) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_0->key = key;
    in_0->in = input_data[i + 1];
    in_0->out = output_data[0][i + 1];

    in_1->key = key;
    in_1->in = input_data[i + 2];
    in_1->out = output_data[1][i + 2];

    in_2->key = key;
    in_2->in = input_data[i];
    in_2->out = output_data[2][i];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // Wait for compute to end
  while (!(jobqueue_0.empty() && jobqueue_1.empty() && jobqueue_2.empty()))
    continue;

  // Clear cache
  clear_cache(input_data);

  // Distribute #3
  for (int i = 0; i < output_data[0].size(); i += 3) {
    auto in_0 = new InputData;
    auto in_1 = new InputData;
    auto in_2 = new InputData;

    in_0->key = key;
    in_0->in = input_data[i + 2];
    in_0->out = output_data[0][i + 2];

    in_1->key = key;
    in_1->in = input_data[i];
    in_1->out = output_data[1][i];

    in_2->key = key;
    in_2->in = input_data[i + 1];
    in_2->out = output_data[2][i + 1];

    jobqueue_0.push(in_0);
    jobqueue_1.push(in_1);
    jobqueue_2.push(in_2);
  }

  // All jobs pushed, send signal to end after compute done
  jobs_done = true;

  // Wait for threads to end
  tmr_0.join();
  tmr_1.join();
  tmr_2.join();

  // Compare data
  int count = diff_data(output_data);

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
