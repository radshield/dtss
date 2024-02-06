#include "dtss.h"
#include <thread>

int rad_hard_compute(OutputData *output_format,
                     std::unordered_set<InputData *> (*partitioner)(),
                     OutputData (*processor)(InputData *)) {
  // Partitioner creates input data and assigns conflicts for each dataset
  auto input_set = (*partitioner)();

  // TODO: scheduling for the thing, based on conflicts
  // TODO: N-coloring to schedule? but how do with rad hardening

  return 0;
}
