#ifndef DTSS_H
#define DTSS_H

#include <unordered_set>

struct InputData {
  size_t data_size;
  void* data_ptr;

  std::unordered_set<InputData *> conflicts;

  virtual ~InputData() {}
};

struct OutputData {
  virtual int g();
  virtual ~OutputData() {}
};

int dtss_compute(OutputData *output_format,
                 std::unordered_set<InputData *> (*partitioner)(),
                 OutputData (*processor)(InputData *));

#endif
