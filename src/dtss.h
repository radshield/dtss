#ifndef DTSS_H
#define DTSS_H

#include <boost/lockfree/spsc_queue.hpp>
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
  boost::lockfree::spsc_queue<InputData *> jobqueue_0;
  boost::lockfree::spsc_queue<InputData *> jobqueue_1;
  boost::lockfree::spsc_queue<InputData *> jobqueue_2;

  std::unordered_map<InputData *, std::unordered_set<InputData *>> conflicts;
  std::unordered_map<InputData *, size_t> compute_sets;

  // Create the graph expressing conflicts between inputs
  void build_conflicts_list(std::unordered_set<InputData *> &input_data);

  // Create compute sets of non-conflicting inputs
  void build_compute_sets();

  // Worker process that recieves data to be processed
  void worker_process(boost::lockfree::spsc_queue<InputData *> *job_queue,
                      OutputData (*processor)(InputData *));

  // Create workers and assign them to processes
  void orchestrator_process(OutputData (*processor)(InputData *));

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
