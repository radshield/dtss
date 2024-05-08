#ifndef DTSS_H
#define DTSS_H

#include <boost/lockfree/spsc_queue.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

typedef std::pair<size_t, void *> DTSSInput;

enum CoreAffinity {
  cpu0,
  cpu1,
  cpu2
};

struct InputData {
  CoreAffinity core_affinity;
  std::vector<DTSSInput> inputs;
};

struct OutputData {
  virtual int g();
  virtual ~OutputData() {}
};

class DTSSInstance {
private:
  bool jobs_done = false;

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
      void (*partitioner)(),
      OutputData (*processor)(InputData *));
};

#endif
