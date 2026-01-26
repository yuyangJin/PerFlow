// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_PMU_SAMPLER_H_
#define PERFLOW_SAMPLING_PMU_SAMPLER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/static_hash_map.h"

namespace perflow {
namespace sampling {

/// Default sampling frequency (samples per second)
constexpr uint64_t kDefaultSamplingFrequency = 100;

/// Default overflow threshold for PMU events
constexpr uint64_t kDefaultOverflowThreshold = 10000000;  // 10M cycles

/// Maximum number of PMU events to monitor simultaneously
constexpr size_t kMaxPmuEvents = 8;

/// PMU event types commonly available on ARMv9
enum class PmuEventType : uint32_t {
  kCpuCycles = 0x0011,           // CPU cycles
  kInstructions = 0x0008,        // Instructions retired
  kCacheMisses = 0x0003,         // Cache misses
  kBranchMisses = 0x0010,        // Branch mispredictions
  kL1DCacheAccess = 0x0004,      // L1 data cache access
  kL1DCacheMiss = 0x0003,        // L1 data cache miss
  kL2CacheAccess = 0x0016,       // L2 cache access
  kL2CacheMiss = 0x0017,         // L2 cache miss
  kBusCycles = 0x001D,           // Bus cycles
  kMemoryAccess = 0x0013,        // Memory access
  kCustom = 0xFFFF               // Custom event code
};

/// Configuration for PMU sampling
struct PmuSamplerConfig {
  /// Sampling frequency in Hz (samples per second)
  uint64_t frequency;

  /// Primary event to sample on
  PmuEventType primary_event;

  /// Overflow threshold for the primary event
  uint64_t overflow_threshold;

  /// Output directory for data files
  char output_directory[256];

  /// Base filename for output files
  char output_filename[128];

  /// Whether to compress output files
  bool compress_output;

  /// Flush interval in seconds (0 = only flush on shutdown)
  uint32_t flush_interval_seconds;

  /// Maximum stack depth to capture
  size_t max_stack_depth;

  /// Enable stack unwinding (if false, only capture IP)
  bool enable_stack_unwinding;

  /// Default constructor with sensible defaults
  PmuSamplerConfig() noexcept
      : frequency(kDefaultSamplingFrequency),
        primary_event(PmuEventType::kCpuCycles),
        overflow_threshold(kDefaultOverflowThreshold),
        compress_output(false),
        flush_interval_seconds(0),
        max_stack_depth(kDefaultMaxStackDepth),
        enable_stack_unwinding(true) {
    std::strcpy(output_directory, "/tmp");
    std::strcpy(output_filename, "perflow_samples");
  }
};

/// Status of the PMU sampler
enum class SamplerStatus {
  kUninitialized = 0,  // Not yet initialized
  kInitialized = 1,    // Initialized but not running
  kRunning = 2,        // Actively sampling
  kStopped = 3,        // Stopped but not cleaned up
  kError = 4           // Error state
};

/// PMU sampler result codes
enum class SamplerResult {
  kSuccess = 0,
  kErrorNotSupported = 1,
  kErrorPermission = 2,
  kErrorInitialization = 3,
  kErrorConfiguration = 4,
  kErrorOverflow = 5,
  kErrorInternal = 6
};

/// PmuSampler provides PMU-based sampling for performance analysis.
/// This is the main interface for runtime data collection.
///
/// @tparam MaxStackDepth Maximum call stack depth
/// @tparam MapCapacity Capacity of the sample storage hash map
template <size_t MaxStackDepth = kDefaultMaxStackDepth,
          size_t MapCapacity = kDefaultMapCapacity>
class PmuSampler {
 public:
  using StackType = CallStack<MaxStackDepth>;
  using MapType = StaticHashMap<StackType, uint64_t, MapCapacity>;

  /// Default constructor
  PmuSampler() noexcept
      : status_(SamplerStatus::kUninitialized),
        sample_count_(0),
        overflow_count_(0),
        perf_fd_(-1),
        custom_handler_(nullptr) {}

  /// Destructor - ensures proper cleanup
  ~PmuSampler() noexcept { cleanup(); }

  // Disable copy operations
  PmuSampler(const PmuSampler&) = delete;
  PmuSampler& operator=(const PmuSampler&) = delete;

  /// Initialize the sampler with the given configuration
  /// @param config Sampler configuration
  /// @return Result code indicating success or failure type
  SamplerResult initialize(const PmuSamplerConfig& config) noexcept {
    if (status_ != SamplerStatus::kUninitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }

    config_ = config;

    // Initialize the sample storage
    samples_.clear();

    // Setup perf event (placeholder - actual implementation would use
    // perf_event_open syscall or PAPI library)
    SamplerResult result = setupPerfEvent();
    if (result != SamplerResult::kSuccess) {
      status_ = SamplerStatus::kError;
      return result;
    }

    status_ = SamplerStatus::kInitialized;
    return SamplerResult::kSuccess;
  }

  /// Start sampling
  /// @return Result code
  SamplerResult start() noexcept {
    if (status_ != SamplerStatus::kInitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }

    // Enable perf event (placeholder)
    if (!enablePerfEvent()) {
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorInternal;
    }

    status_ = SamplerStatus::kRunning;
    return SamplerResult::kSuccess;
  }

  /// Stop sampling
  /// @return Result code
  SamplerResult stop() noexcept {
    if (status_ != SamplerStatus::kRunning) {
      return SamplerResult::kSuccess;  // Already stopped
    }

    // Disable perf event (placeholder)
    disablePerfEvent();

    status_ = SamplerStatus::kStopped;
    return SamplerResult::kSuccess;
  }

  /// Cleanup all resources
  void cleanup() noexcept {
    stop();
    closePerfEvent();
    status_ = SamplerStatus::kUninitialized;
  }

  /// Flush collected data to disk
  /// @return Data operation result
  DataResult flush() noexcept {
    DataExporter exporter(config_.output_directory, config_.output_filename,
                          config_.compress_output);
    return exporter.exportData(samples_);
  }

  /// Handle PMU overflow event (called from signal handler or callback)
  /// This captures the current call stack and updates the sample map.
  /// Must be signal-safe (no dynamic allocation).
  void handleOverflow() noexcept {
    overflow_count_.fetch_add(1, std::memory_order_relaxed);

    // Capture current call stack
    StackType stack;
    if (config_.enable_stack_unwinding) {
      captureCallStack(stack);
    } else {
      // Just capture instruction pointer
      void* ip = __builtin_return_address(0);
      stack.push(reinterpret_cast<uintptr_t>(ip));
    }

    // Update sample count for this call stack
    samples_.increment(stack);
    sample_count_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Get current sampler status
  SamplerStatus status() const noexcept { return status_; }

  /// Get total sample count
  uint64_t sampleCount() const noexcept {
    return sample_count_.load(std::memory_order_relaxed);
  }

  /// Get overflow count
  uint64_t overflowCount() const noexcept {
    return overflow_count_.load(std::memory_order_relaxed);
  }

  /// Get the number of unique call stacks captured
  size_t uniqueStackCount() const noexcept { return samples_.size(); }

  /// Get the configuration
  const PmuSamplerConfig& config() const noexcept { return config_; }

  /// Access the raw sample data (for custom analysis)
  const MapType& samples() const noexcept { return samples_; }

  /// Update configuration at runtime
  /// @param config New configuration
  /// @return Result code
  SamplerResult updateConfig(const PmuSamplerConfig& config) noexcept {
    if (status_ == SamplerStatus::kRunning) {
      // Some parameters can be updated while running
      config_.frequency = config.frequency;
      config_.overflow_threshold = config.overflow_threshold;

      // Update perf event parameters
      return updatePerfEventConfig();
    }

    // When not running, full reconfiguration is allowed
    config_ = config;
    return SamplerResult::kSuccess;
  }

  /// Register a custom overflow handler
  /// @param handler Function pointer: void handler(PmuSampler* sampler)
  void setOverflowHandler(void (*handler)(PmuSampler*)) noexcept {
    custom_handler_ = handler;
  }

 private:
  // Sampler state
  SamplerStatus status_;
  PmuSamplerConfig config_;

  // Sample storage (statically allocated)
  MapType samples_;

  // Counters
  std::atomic<uint64_t> sample_count_;
  std::atomic<uint64_t> overflow_count_;

  // Perf file descriptor (Linux perf_event)
  int perf_fd_;

  // Custom overflow handler
  void (*custom_handler_)(PmuSampler*);

  /// Setup perf event for sampling
  SamplerResult setupPerfEvent() noexcept {
    // Placeholder for actual perf_event_open implementation
    // In a real implementation, this would:
    // 1. Open /dev/cpu/*/msr or use perf_event_open syscall
    // 2. Configure the PMU event and sampling parameters
    // 3. Set up signal handler for overflow notifications

    // For now, simulate successful setup
    perf_fd_ = 0;  // Placeholder
    return SamplerResult::kSuccess;
  }

  /// Enable perf event
  bool enablePerfEvent() noexcept {
    // Placeholder - would call ioctl(perf_fd_, PERF_EVENT_IOC_ENABLE, 0)
    return perf_fd_ >= 0;
  }

  /// Disable perf event
  void disablePerfEvent() noexcept {
    // Placeholder - would call ioctl(perf_fd_, PERF_EVENT_IOC_DISABLE, 0)
  }

  /// Close perf event
  void closePerfEvent() noexcept {
    if (perf_fd_ > 0) {
      // Placeholder - would call close(perf_fd_)
      perf_fd_ = -1;
    }
  }

  /// Update perf event configuration
  SamplerResult updatePerfEventConfig() noexcept {
    // Placeholder - would update sampling period
    return SamplerResult::kSuccess;
  }

  /// Capture the current call stack
  /// Uses frame pointer unwinding or DWARF unwinding
  void captureCallStack(StackType& stack) noexcept {
    // Use GCC/Clang built-in frame address for basic unwinding
    // In a production implementation, this would use libunwind or
    // custom frame pointer walking

    void* frame_ptrs[MaxStackDepth];
    size_t depth = 0;

    // Walk the stack using built-in functions
    // Note: This requires -fno-omit-frame-pointer for accurate results
#define CAPTURE_FRAME(N)                            \
  do {                                              \
    if (depth < MaxStackDepth) {                    \
      void* fp = __builtin_frame_address(N);        \
      if (fp == nullptr) break;                     \
      void* ra = __builtin_return_address(N);       \
      if (ra == nullptr) break;                     \
      frame_ptrs[depth++] = ra;                     \
    }                                               \
  } while (0)

    // Capture up to 16 frames using built-ins (limited by compiler)
    CAPTURE_FRAME(0);
    CAPTURE_FRAME(1);
    CAPTURE_FRAME(2);
    CAPTURE_FRAME(3);
    CAPTURE_FRAME(4);
    CAPTURE_FRAME(5);
    CAPTURE_FRAME(6);
    CAPTURE_FRAME(7);
    CAPTURE_FRAME(8);
    CAPTURE_FRAME(9);
    CAPTURE_FRAME(10);
    CAPTURE_FRAME(11);
    CAPTURE_FRAME(12);
    CAPTURE_FRAME(13);
    CAPTURE_FRAME(14);
    CAPTURE_FRAME(15);

#undef CAPTURE_FRAME

    // Set the captured frames
    for (size_t i = 0; i < depth; ++i) {
      stack.push(reinterpret_cast<uintptr_t>(frame_ptrs[i]));
    }
  }
};

/// Type alias for common configurations
using DefaultPmuSampler = PmuSampler<kDefaultMaxStackDepth, kDefaultMapCapacity>;

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_PMU_SAMPLER_H_
