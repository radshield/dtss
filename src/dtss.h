#ifndef DTSS_H
#define DTSS_H

#include <unordered_map>
#include <unordered_set>

struct InputData {
  size_t data_size;
  void *data_ptr;
};

struct OutputData {
  virtual int g();
  virtual ~OutputData() {}
};

class DTSSInstance {
private:
  std::unordered_map<InputData *, std::unordered_set<InputData *>> conflicts;

  void build_conflicts_list(std::unordered_set<InputData *> &input_data);

public:
  int dtss_compute(OutputData *output_format,
                   std::unordered_set<InputData *> dataset,
                   OutputData (*processor)(InputData *));
  int dtss_compute(
      OutputData *output_format,
      std::unordered_map<InputData *, std::unordered_set<InputData *>> (
          *partitioner)(),
      OutputData (*processor)(InputData *));
};

#endif
