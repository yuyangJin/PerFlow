// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

#ifndef PERFLOW_SAMPLING_DATA_COLLECTION_H_
#define PERFLOW_SAMPLING_DATA_COLLECTION_H_

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "sampler.h"

namespace perflow {
namespace sampling {

/**
 * @brief Output format for sample data
 */
enum class OutputFormat {
  kText,        // Human-readable text format
  kBinary,      // Compact binary format
  kCompressed   // Compressed binary format
};

/**
 * @brief Thread-safe runtime performance data collection module
 */
class DataCollection {
 public:
  DataCollection();
  ~DataCollection();

  /**
   * @brief Initialize the data collection module
   * @param output_path Path prefix for output files
   * @param format Output format
   * @param rank MPI rank (-1 for non-MPI)
   * @return 0 on success, negative error code on failure
   */
  int Initialize(const std::string& output_path, OutputFormat format, int rank = -1);

  /**
   * @brief Add a sample to the collection (thread-safe)
   * @param sample Sample to add
   */
  void AddSample(const Sample& sample);

  /**
   * @brief Add a sample to the collection (signal-safe, lock-free)
   *
   * This version uses a lock-free ring buffer and is safe to call
   * from signal handlers.
   *
   * @param timestamp Sample timestamp
   * @param event_values Array of event counter values
   * @param num_events Number of events
   * @param stack Call stack addresses
   * @param stack_depth Depth of call stack
   * @param thread_id Thread ID
   */
  void AddSampleSignalSafe(uint64_t timestamp, const int64_t* event_values,
                           int num_events, void* const* stack,
                           int stack_depth, int thread_id);

  /**
   * @brief Get all collected samples
   * @return Vector of samples
   */
  const std::vector<Sample>& GetSamples() const;

  /**
   * @brief Get the number of samples collected
   * @return Number of samples
   */
  size_t GetSampleCount() const;

  /**
   * @brief Write samples to output file
   * @return 0 on success, negative error code on failure
   */
  int WriteOutput();

  /**
   * @brief Clear all collected samples
   */
  void Clear();

  /**
   * @brief Flush pending signal-safe samples to main storage
   *
   * This should be called periodically from a safe context to move
   * samples from the lock-free buffer to the main sample vector.
   */
  void FlushSignalSafeSamples();

 private:
  int WriteTextOutput();
  int WriteBinaryOutput();
  int WriteCompressedOutput();

  std::vector<Sample> samples_;
  std::mutex mutex_;
  std::string output_path_;
  OutputFormat format_;
  int rank_;

  // Lock-free ring buffer for signal-safe sample collection
  static constexpr size_t kRingBufferSize = 8192;
  struct RawSample {
    uint64_t timestamp;
    int64_t event_values[3];
    void* stack[64];
    int stack_depth;
    int thread_id;
    bool valid;
  };
  RawSample* ring_buffer_;
  std::atomic<size_t> ring_head_;
  std::atomic<size_t> ring_tail_;
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_DATA_COLLECTION_H_
