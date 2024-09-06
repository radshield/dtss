#include "dtss.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <tuple>
#include <xmmintrin.h>

void clear_cache(DTSSInput *in) {
  for (int i = 0; i <= in->first; i += 64)
    _mm_clflush(static_cast<char *>(in->second) + i);
}

void clear_cache(InputData *in) {
  for (auto input : in->inputs)
    clear_cache(&input);
}

bool InputData::operator==(InputData &b) {
  if (b.inputs == this->inputs)
    return true;
  else
    return false;
}

void DTSSInstance::build_conflicts_list(
    std::unordered_set<InputData *> &input_data) {
  std::unordered_map<DTSSInput, int> use_count;
  std::unordered_map<InputData *, int> region_count;
  std::multimap<size_t, std::tuple<InputData *, int, bool>> memory_regions;
  std::unordered_map<InputData *, std::unordered_set<int>> active_regions;
  std::unordered_map<DTSSInput,
                     std::tuple<DTSSInput *, DTSSInput *, DTSSInput *>>
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

  // Duplicate across executors if more than 5% of the jobs use this
  for (auto use : use_count) {
    if (use.second > input_data.size() / 20) {
      // Duplicate elements
      duplicate_uses[use.first] = std::make_tuple(
          &(use.first), static_cast<DTSSInput *>(malloc(use.first.first)),
          static_cast<DTSSInput *>(malloc(use.first.first)));

      // Add to list of duplicates
      duplicates.insert(std::get<0>(duplicate_uses[use.first]));
      duplicates.insert(std::get<1>(duplicate_uses[use.first]));
      duplicates.insert(std::get<2>(duplicate_uses[use.first]));

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
      if (active_regions[std::get<0>(it->second)].contains(
              std::get<2>(it->second)))
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
    for (size_t i = 0; i < MAX_INPUT_SETS; i++) {
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
    void (*processor)(InputData *)) {

  while (!jobs_done) {
    if (job_queue->read_available() == 0)
      continue;
    else {
      // Input available in job queue, process
      InputData *data = job_queue->front();
      processor(data);

      // Clear data from cache once processed
      for (auto input : data->inputs)
        if (this->duplicates.find(&input) != this->duplicates.end())
          clear_cache(&input);

      // Remove from queue once processed
      job_queue->pop();
    }
  }
}

void DTSSInstance::orchestrator_process(void (*processor)(InputData *)) {
  cpu_set_t cpuset;
  int r;
  size_t max_compute_set;

  for (auto entry : compute_sets) {
    if (entry.second > max_compute_set)
      max_compute_set = entry.second;
  }

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

  for (size_t i = 0; i < max_compute_set; i++) {
    // Load in compute set
    for (auto compute : this->compute_sets) {
      if (compute.second == i) {
        switch (compute.first->core_affinity) {
        case cpu0:
          this->jobqueue_0.push(compute.first);
          break;
        case cpu1:
          this->jobqueue_1.push(compute.first);
          break;
        case cpu2:
          this->jobqueue_2.push(compute.first);
          break;
        }
      }
    }

    // Wait until all jobs done before going to the next compute set
    while (this->jobqueue_0.read_available() != 0) {
    }
    while (this->jobqueue_1.read_available() != 0) {
    }
    while (this->jobqueue_2.read_available() != 0) {
    }

    // Compare compute sets to ensure correctness
    std::unordered_map<InputData *,
                       std::tuple<InputData *, InputData *, InputData *>>
        comparators;
    for (auto compute : this->compute_sets) {
      if (compute.second == i) {
        // Add to existing
        if (comparators.find(compute.first) == comparators.end())
          comparators[compute.first] =
              std::make_tuple<InputData *, InputData *, InputData *>(
                  nullptr, nullptr, nullptr);

        switch (compute.first->core_affinity) {
        case cpu0:
          std::get<0>(comparators.find(compute.first)->second) = compute.first;
        case cpu1:
          std::get<1>(comparators.find(compute.first)->second) = compute.first;
        case cpu2:
          std::get<2>(comparators.find(compute.first)->second) = compute.first;
        }
      }
    }

    for (auto i : comparators) {
      if (memcmp(std::get<0>(i.second)->output.second,
                 std::get<1>(i.second)->output.second,
                 i.first->output.first) != 0) {

        throw std::runtime_error(
            "potential radiation error: results mismatch between cpu0, cpu1");
      }
      if (memcmp(std::get<1>(i.second)->output.second,
                 std::get<2>(i.second)->output.second,
                 i.first->output.first) != 0) {
        throw std::runtime_error(
            "potential radiation error: results mismatch between cpu1, cpu2");
      }
      if (memcmp(std::get<0>(i.second)->output.second,
                 std::get<2>(i.second)->output.second,
                 i.first->output.first) != 0) {
        throw std::runtime_error(
            "potential radiation error: results mismatch between cpu0, cpu2");
      }
    }
  }

  // All jobs pushed, send signal to end after compute done
  this->jobs_done = true;

  // Wait for threads to end
  tmr_0.join();
  tmr_1.join();
  tmr_2.join();
}

int DTSSInstance::dtss_compute(std::unordered_set<InputData *> dataset,
                               void (*processor)(InputData *)) {
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
    new_input_cpu1->output.second = malloc((*i)->output.first);
    input_data.insert(new_input_cpu1);

    InputData *new_input_cpu2 = new InputData();
    *new_input_cpu2 = *(*i);
    new_input_cpu2->core_affinity = cpu2;
    new_input_cpu2->output.second = malloc((*i)->output.first);
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

int DTSSInstance::dtss_compute(void (*partitioner)(),
                               void (*processor)(InputData *)) {
  // User-provided partitioner function creates input data and assigns conflicts
  partitioner();

  // Generate compute sets by coloring the graph greedily
  this->build_compute_sets();

  // Call orchestrator process and start workers
  this->orchestrator_process(processor);

  return 0;
}
