// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_SAMPLER_H_
#define PERFLOW_SAMPLING_SAMPLER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/static_hash_map.h"

namespace perflow {
namespace sampling {

constexpr uint64_t kDefaultSamplingFrequency = 100;
constexpr uint64_t kDefaultOverflowThreshold = 10000000;
constexpr size_t kMaxPmuEvents = 8;

enum class PmuEventType : uint32_t {
  kCpuCycles = 0x0011,
  kInstructions = 0x0008,
  kCacheMisses = 0x0003,
  kBranchMisses = 0x0010,
  kL1DCacheAccess = 0x0004,
  kL1DCacheMiss = 0x0003,
  kL2CacheAccess = 0x0016,
  kL2CacheMiss = 0x0017,
  kBusCycles = 0x001D,
  kMemoryAccess = 0x0013,
  kCustom = 0xFFFF
};

struct PmuSamplerConfig {
  uint64_t frequency;
  PmuEventType primary_event;
  uint64_t overflow_threshold;
  char output_directory[256];
  char output_filename[128];
  bool compress_output;
  uint32_t flush_interval_seconds;
  size_t max_stack_depth;
  bool enable_stack_unwinding;

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

enum class SamplerStatus {
  kUninitialized = 0,
  kInitialized = 1,
  kRunning = 2,
  kStopped = 3,
  kError = 4
};

enum class SamplerResult {
  kSuccess = 0,
  kErrorNotSupported = 1,
  kErrorPermission = 2,
  kErrorInitialization = 3,
  kErrorConfiguration = 4,
  kErrorOverflow = 5,
  kErrorInternal = 6
};

inline const char* statusToString(SamplerStatus status) noexcept {
  switch (status) {
    case SamplerStatus::kUninitialized: return "Uninitialized";
    case SamplerStatus::kInitialized: return "Initialized";
    case SamplerStatus::kRunning: return "Running";
    case SamplerStatus::kStopped: return "Stopped";
    case SamplerStatus::kError: return "Error";
    default: return "Unknown";
  }
}

inline const char* resultToString(SamplerResult result) noexcept {
  switch (result) {
    case SamplerResult::kSuccess: return "Success";
    case SamplerResult::kErrorNotSupported: return "Not Supported";
    case SamplerResult::kErrorPermission: return "Permission Denied";
    case SamplerResult::kErrorInitialization: return "Initialization Error";
    case SamplerResult::kErrorConfiguration: return "Configuration Error";
    case SamplerResult::kErrorOverflow: return "Overflow Error";
    case SamplerResult::kErrorInternal: return "Internal Error";
    default: return "Unknown";
  }
}

template <size_t MaxStackDepth = kDefaultMaxStackDepth,
          size_t MapCapacity = kDefaultMapCapacity>
class Sampler {
 public:
  using StackType = CallStack<MaxStackDepth>;
  using MapType = StaticHashMap<StackType, uint64_t, MapCapacity>;

  Sampler() = default;
  virtual ~Sampler() = default;

  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  virtual SamplerResult initialize(const PmuSamplerConfig& config) noexcept = 0;
  virtual SamplerResult finalize() noexcept = 0;
  virtual SamplerResult start() noexcept = 0;
  virtual SamplerResult stop() noexcept = 0;
  virtual void cleanup() noexcept = 0;
  virtual DataResult flush() noexcept = 0;
  virtual void handleOverflow() noexcept = 0;

  virtual SamplerStatus status() const noexcept = 0;
  virtual uint64_t sampleCount() const noexcept = 0;
  virtual uint64_t overflowCount() const noexcept = 0;
  virtual size_t uniqueStackCount() const noexcept = 0;
  virtual const PmuSamplerConfig& config() const noexcept = 0;
  virtual const MapType& samples() const noexcept = 0;
  virtual SamplerResult updateConfig(const PmuSamplerConfig& config) noexcept = 0;
};

constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 1;
constexpr int kVersionPatch = 0;

inline const char* getVersionString() noexcept { return "0.1.0"; }

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLER_H_
