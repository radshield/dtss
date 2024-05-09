#include "dtss.h"

#include <iostream>
#include <map>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <tuple>
#include <utility>

void DTSSInstance::build_conflicts_list(
    std::unordered_set<InputData *> &input_data) {
  std::unordered_map<DTSSInput, int> use_count;
  std::unordered_map<InputData *, int> region_count;
  std::multimap<size_t, std::tuple<InputData *, int, bool>> memory_regions;
  std::unordered_map<InputData *, std::unordered_set<int>> active_regions;
  std::unordered_map<DTSSInput, std::tuple<void *, void *, void *>>
      duplicate_uses;

  // Check for overlapping inputs
  for (auto input : input_data) {
    for (auto dtss_input : input->inputs) {
      if (use_count.contains(dtss_input))
        use_count[dtss_input] += 1;
      else
        use_count[dtss_input] = 1;
    }
  }

  // Duplicate across executors if more than 1/2 of the jobs use this
  for (auto use : use_count) {
    if (use.second > input_data.size() / 2) {
      // Duplicate elements
      duplicate_uses[use.first] = std::make_tuple(
          use.first.second, malloc(use.first.first), malloc(use.first.first));

      // Copy data to duplicate elements
      memcpy(std::get<1>(duplicate_uses[use.first]), use.first.second,
             use.first.first);
      memcpy(std::get<2>(duplicate_uses[use.first]), use.first.second,
             use.first.first);
    }
  }

  // Build a map of all the memory regions that are being used
  for (auto input : input_data) {
    // Insert an interval at each start and end of the memory region
    for (auto dtss_input : input->inputs) {
      // Only check if not duplicated already
      if (!use_count.contains(dtss_input)) {
        // Set region count
        if (region_count.contains(input))
          region_count[input] = 1;
        else
          region_count[input] += 1;

        // Insert
        memory_regions.insert(
            std::make_pair(reinterpret_cast<size_t>(dtss_input.second),
                           std::make_tuple(input, false, region_count[input])));
        memory_regions.insert(std::make_pair(
            reinterpret_cast<size_t>(dtss_input.second) + dtss_input.first,
            std::make_tuple(input, true, region_count[input])));
      }
    }
  }

  // Iterate through memory intervals and log intersections in conflicts
  for (auto it = memory_regions.begin(); it != memory_regions.end(); ++it) {
    // Add current memory region into list of active regions as needed
    if (active_regions.contains(std::get<0>(it->second))) {
      if (active_regions[std::get<0>(it->second)].contains(std::get<2>(it->second)))
        active_regions[std::get<0>(it->second)].insert(std::get<2>(it->second));
    } else {
      active_regions[std::get<0>(it->second)] = {};
      active_regions[std::get<0>(it->second)].insert(std::get<2>(it->second));
    }

    // Insert all currently active regions into conflict graph
    for (auto input1 : active_regions)
      for (auto input2 : active_regions)
        if (input1 != input2)
          this->conflicts[input1.first].insert(input2.first);

    // If this is tagges as an end file, it's the end of the memory region of
    // this input
    if (std::get<1>(it->second))
      active_regions[std::get<0>(it->second)].erase(std::get<2>(it->second));

    // If the set of active regions is now empty, clear it out
    if (active_regions[std::get<0>(it->second)].empty())
      active_regions.erase(std::get<0>(it->second));
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
  auto input_data = dataset;

  // Set original copies to CPU 0
  for (auto i = input_data.begin(); i != input_data.end(); i++) {
    (*i)->core_affinity = cpu0;
  }

  // Create duplicates for each CPU
  for (auto i = dataset.begin(); i != dataset.end(); i++) {
    InputData *new_input_cpu1 = new InputData();
    *new_input_cpu1 = *(*i);
    new_input_cpu1->core_affinity = cpu1;
    input_data.insert(new_input_cpu1);

    InputData *new_input_cpu2 = new InputData();
    *new_input_cpu2 = *(*i);
    new_input_cpu2->core_affinity = cpu2;
    input_data.insert(new_input_cpu2);
  }

  // Default artitioner creates input data and assigns conflicts
  this->build_conflicts_list(input_data);

  // Generate compute sets by coloring the graph greedily
  this->build_compute_sets();

  // Call orchestrator process and start workers
  this->orchestrator_process(processor);

  return 0;
}

int DTSSInstance::dtss_compute(OutputData *output_format, void (*partitioner)(),
                               OutputData (*processor)(InputData *)) {
  // User-provided partitioner function creates input data and assigns conflicts
  partitioner();

  // Generate compute sets by coloring the graph greedily
  this->build_compute_sets();

  // Call orchestrator process and start workers
  this->orchestrator_process(processor);

  return 0;
}
