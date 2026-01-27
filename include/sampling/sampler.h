// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

#ifndef PERFLOW_SAMPLING_SAMPLER_H_
#define PERFLOW_SAMPLING_SAMPLER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace perflow {
namespace sampling {

/**
 * @brief Structure to hold a single performance sample
 */
struct Sample {
  uint64_t timestamp;              // Timestamp in nanoseconds
  int64_t event_values[3];         // Values of hardware counters
  std::vector<void*> call_stack;   // Captured call stack
  int rank;                        // MPI rank (-1 if not MPI)
  int thread_id;                   // Thread ID
};

/**
 * @brief Configuration for the sampler
 */
struct SamplerConfig {
  int sampling_frequency;          // Sampling frequency in Hz (default: 1000)
  bool enable_call_stack;          // Enable call stack capture
  bool compress_output;            // Enable compressed output format
  std::string output_path;         // Output file path prefix
  std::vector<int> events;         // PAPI event codes to monitor

  SamplerConfig()
      : sampling_frequency(1000),
        enable_call_stack(true),
        compress_output(false),
        output_path("./perflow_samples") {}
};

/**
 * @brief Abstract base class for performance samplers
 */
class Sampler {
 public:
  virtual ~Sampler() = default;

  /**
   * @brief Initialize the sampler with given configuration
   * @param config Sampler configuration
   * @return 0 on success, negative error code on failure
   */
  virtual int Initialize(const SamplerConfig& config) = 0;

  /**
   * @brief Start sampling
   * @return 0 on success, negative error code on failure
   */
  virtual int Start() = 0;

  /**
   * @brief Stop sampling
   * @return 0 on success, negative error code on failure
   */
  virtual int Stop() = 0;

  /**
   * @brief Finalize and cleanup resources
   * @return 0 on success, negative error code on failure
   */
  virtual int Finalize() = 0;

  /**
   * @brief Get collected samples
   * @return Vector of collected samples
   */
  virtual const std::vector<Sample>& GetSamples() const = 0;

  /**
   * @brief Write samples to output file
   * @return 0 on success, negative error code on failure
   */
  virtual int WriteOutput() = 0;

  /**
   * @brief Get the number of samples collected
   * @return Number of samples
   */
  virtual size_t GetSampleCount() const = 0;

  /**
   * @brief Check if sampler is currently active
   * @return true if sampling is active
   */
  virtual bool IsActive() const = 0;
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLER_H_
