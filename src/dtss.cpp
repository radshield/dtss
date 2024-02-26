#include "dtss.h"

#include <iostream>
#include <map>
#include <pthread.h>
#include <sched.h>
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

void DTSSInstance::build_compute_sets() {
  // Loop through all nodes previously created
  for (const auto &[target_node, conflicts] : this->conflicts) {
    // Find all previous conflicting compute sets
    std::unordered_set<size_t> conflicting_sets;
    for (auto conflict_node : conflicts)
      if (compute_sets.find(conflict_node) != compute_sets.end())
        conflicting_sets.insert(compute_sets.at(conflict_node));

    // Go through and find first compute set that isn't listed
    for (size_t i = 0; i < 65535; i++) {
      if (conflicting_sets.find(i) == conflicting_sets.end()) {
        // First compute set not in the list of conflicting sets
        this->compute_sets[target_node] = i;
        break;
      }
    }
  }
}

void DTSSInstance::worker_process(
    boost::lockfree::spsc_queue<InputData *> *job_queue,
    OutputData (*processor)(InputData *)) {

  while (job_queue->empty() || !jobs_done) {
    if (job_queue->empty())
      continue;
    else {
      // Process is in job queue, remove from queue and process
      InputData *data = job_queue->front();
      job_queue->pop();
      processor(data);
    }
  }
}

void DTSSInstance::orchestrator_process(OutputData (*processor)(InputData *)) {
  cpu_set_t cpuset;
  int r;

  // Start threads
  std::thread tmr_0(&DTSSInstance::worker_process, this, &this->jobqueue_0,
                    processor);
  CPU_ZERO(&cpuset);
  CPU_SET(1, &cpuset);
  r = pthread_setaffinity_np(tmr_0.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 0 to core" << std::endl;

  std::thread tmr_1(&DTSSInstance::worker_process, this, &this->jobqueue_1,
                    processor);
  CPU_ZERO(&cpuset);
  CPU_SET(2, &cpuset);
  r = pthread_setaffinity_np(tmr_1.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 1 to core" << std::endl;

  std::thread tmr_2(&DTSSInstance::worker_process, this, &this->jobqueue_2,
                    processor);
  CPU_ZERO(&cpuset);
  CPU_SET(3, &cpuset);
  r = pthread_setaffinity_np(tmr_2.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (r != 0)
    std::cerr << "Error binding worker 2 to core" << std::endl;

  // All jobs pushed, send signal to end after compute done
  this->jobs_done = true;

  // Wait for threads to end
  tmr_0.join();
  tmr_1.join();
  tmr_2.join();
}

int DTSSInstance::dtss_compute(OutputData *output_format,
                               std::unordered_set<InputData *> dataset,
                               OutputData (*processor)(InputData *)) {
  // Default artitioner creates input data and assigns conflicts
  this->build_conflicts_list(dataset);

  // Generate compute sets by coloring the graph greedily
  this->build_compute_sets();

  // Call orchestrator process and start workers
  this->orchestrator_process(processor);

  return 0;
}

int DTSSInstance::dtss_compute(
    OutputData *output_format,
    std::unordered_map<InputData *, std::unordered_set<InputData *>> (
        *partitioner)(),
    OutputData (*processor)(InputData *)) {
  // User-provided partitioner function creates input data and assigns conflicts
  conflicts = partitioner();

  // Generate compute sets by coloring the graph greedily
  this->build_compute_sets();

  // Call orchestrator process and start workers
  this->orchestrator_process(processor);

  return 0;
}
