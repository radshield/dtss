#include "dtss.h"

#include <map>
#include <thread>

void DTSSInstance::build_conflicts_list(
    std::unordered_set<InputData *> &input_data) {
  std::multimap<size_t, InputData *> memory_regions;

  // Build a map of all the memory regions that are being built
  for (auto input : input_data) {
    // Insert an interval at each start and end of the memory region
    memory_regions.insert(
        std::pair<size_t, InputData *>((size_t)(input->data_ptr), input));
    memory_regions.insert(std::pair<size_t, InputData *>(
        (size_t)(input->data_ptr) + input->data_size, input));
  }

  // Iterate through memory intervals and log intersections in conflicts
  std::unordered_set<InputData *> active_regions;
  for (auto it = memory_regions.begin(); it != memory_regions.end(); ++it) {
    bool is_original = active_regions.insert(it->second).second;

    // Insert all currently active regions into conflict graph
    for (auto input1 : active_regions)
      for (auto input2 : active_regions)
        if (input1 != input2)
          this->conflicts[input1].insert(input2);

    // If this is the 2nd time we see it, it's the end of the memory region of
    // this dataset
    if (!is_original)
      active_regions.erase(it->second);
  }
}

int DTSSInstance::dtss_compute(OutputData *output_format,
                               std::unordered_set<InputData *> dataset,
                               OutputData (*processor)(InputData *)) {
  // Default artitioner creates input data and assigns conflicts
  this->build_conflicts_list(dataset);

  // TODO: scheduling for the thing, based on conflicts
  // TODO: N-coloring to schedule?

  return 0;
}

int DTSSInstance::dtss_compute(
    OutputData *output_format,
    std::unordered_map<InputData *, std::unordered_set<InputData *>> (
        *partitioner)(),
    OutputData (*processor)(InputData *)) {
  // Partitioner function creates input data and assigns conflicts
  conflicts = partitioner();

  return 0;
}
