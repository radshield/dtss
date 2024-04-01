#ifndef IMG_H
#define IMG_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sys/fcntl.h>
#include <unistd.h>
#include <vector>
#include <x86intrin.h>

#define CHUNK_SZ 8192

int nccscore_data(cv::Mat *img, cv::Mat *match, size_t start_x,
                  size_t start_y) {
  int score = 0;

  // Make sure everything is in bounds
  if (img->rows < start_x + match->rows || img->cols < start_y + match->cols)
    return score;

  for (size_t i = 0; i < match->rows; i++) {
    for (size_t j = 0; j < match->cols; j++) {

      for (size_t k = 0; k < 3; k++) {
        score += img->at<cv::Vec3b>(start_x + i, start_y + j)[k] -
                 match->at<cv::Vec3b>(i, j)[k];
      }
    }
  }

  return score;
}

void clear_cache(cv::Mat *img) {
  for (size_t i = 0; i < img->rows; i++) {
    for (size_t j = 0; j < img->cols; j++) {
      _mm_clflush(std::addressof(img->at<cv::Vec3b>(i, j)[0]));
      _mm_clflush(std::addressof(img->at<cv::Vec3b>(i, j)[1]));
      _mm_clflush(std::addressof(img->at<cv::Vec3b>(i, j)[2]));
    }
  }
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

int diff_data(std::vector<std::vector<std::vector<int>>> &output_data) {
  int count = 0;

  for (int i = 0; i < output_data[0].size(); i++) {
    for (int j = 0; j < output_data[0][0].size(); j++) {
      if (output_data[0][i][j] != output_data[1][i][j] ||
          output_data[1][i][j] != output_data[2][i][j] ||
          output_data[2][i][j] != output_data[0][i][j])
        count++;
    }
  }

  return count;
}

#endif
