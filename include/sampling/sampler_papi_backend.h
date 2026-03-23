// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_SAMPLER_PAPI_BACKEND_H_
#define PERFLOW_SAMPLING_SAMPLER_PAPI_BACKEND_H_

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "sampling/sampler.h"

namespace perflow {
namespace sampling {

template <size_t MaxStackDepth = kDefaultMaxStackDepth,
          size_t MapCapacity = kDefaultMapCapacity>
class PapiBackendSampler : public Sampler<MaxStackDepth, MapCapacity> {
 public:
  using StackType = typename Sampler<MaxStackDepth, MapCapacity>::StackType;
  using MapType = typename Sampler<MaxStackDepth, MapCapacity>::MapType;

  PapiBackendSampler() noexcept
      : status_(SamplerStatus::kUninitialized),
        sample_count_(0),
        overflow_count_(0),
        perf_fd_(-1),
        custom_handler_(nullptr) {}

  ~PapiBackendSampler() noexcept override { cleanup(); }

  SamplerResult initialize(const PmuSamplerConfig& config) noexcept override {
    if (status_ != SamplerStatus::kUninitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }

    config_ = config;
    samples_.clear();

    SamplerResult result = setupPerfEvent();
    if (result != SamplerResult::kSuccess) {
      status_ = SamplerStatus::kError;
      return result;
    }

    status_ = SamplerStatus::kInitialized;
    return SamplerResult::kSuccess;
  }

  SamplerResult finalize() noexcept override {
    stop();
    closePerfEvent();
    status_ = SamplerStatus::kStopped;
    return SamplerResult::kSuccess;
  }

  SamplerResult start() noexcept override {
    if (status_ != SamplerStatus::kInitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }

    if (!enablePerfEvent()) {
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorInternal;
    }

    status_ = SamplerStatus::kRunning;
    return SamplerResult::kSuccess;
  }

  SamplerResult stop() noexcept override {
    if (status_ != SamplerStatus::kRunning) {
      return SamplerResult::kSuccess;
    }
    disablePerfEvent();
    status_ = SamplerStatus::kStopped;
    return SamplerResult::kSuccess;
  }

  void cleanup() noexcept override {
    stop();
    closePerfEvent();
    status_ = SamplerStatus::kUninitialized;
  }

  DataResult flush() noexcept override {
    DataExporter exporter(config_.output_directory, config_.output_filename,
                          config_.compress_output);
    return exporter.exportData(samples_);
  }

  void handleOverflow() noexcept override {
    overflow_count_.fetch_add(1, std::memory_order_relaxed);

    StackType stack;
    if (config_.enable_stack_unwinding) {
      captureCallStack(stack);
    } else {
      void* ip = __builtin_return_address(0);
      stack.push(reinterpret_cast<uintptr_t>(ip));
    }
    samples_.increment(stack);
    sample_count_.fetch_add(1, std::memory_order_relaxed);

    if (custom_handler_) {
      custom_handler_(this);
    }
  }

  SamplerStatus status() const noexcept override { return status_; }
  uint64_t sampleCount() const noexcept override {
    return sample_count_.load(std::memory_order_relaxed);
  }
  uint64_t overflowCount() const noexcept override {
    return overflow_count_.load(std::memory_order_relaxed);
  }
  size_t uniqueStackCount() const noexcept override { return samples_.size(); }
  const PmuSamplerConfig& config() const noexcept override { return config_; }
  const MapType& samples() const noexcept override { return samples_; }

  SamplerResult updateConfig(const PmuSamplerConfig& config) noexcept override {
    if (status_ == SamplerStatus::kRunning) {
      config_.frequency = config.frequency;
      config_.overflow_threshold = config.overflow_threshold;
      return updatePerfEventConfig();
    }
    config_ = config;
    return SamplerResult::kSuccess;
  }

  void setOverflowHandler(void (*handler)(PapiBackendSampler*)) noexcept {
    custom_handler_ = handler;
  }

 private:
  SamplerStatus status_;
  PmuSamplerConfig config_;
  MapType samples_;
  std::atomic<uint64_t> sample_count_;
  std::atomic<uint64_t> overflow_count_;
  int perf_fd_;
  void (*custom_handler_)(PapiBackendSampler*);

  SamplerResult setupPerfEvent() noexcept {
    perf_fd_ = 0;
    return SamplerResult::kSuccess;
  }

  bool enablePerfEvent() noexcept { return perf_fd_ >= 0; }
  void disablePerfEvent() noexcept {}
  void closePerfEvent() noexcept {
    if (perf_fd_ > 0) {
      perf_fd_ = -1;
    }
  }
  SamplerResult updatePerfEventConfig() noexcept { return SamplerResult::kSuccess; }

  void captureCallStack(StackType& stack) noexcept {
    void* frame_ptrs[MaxStackDepth];
    size_t depth = 0;

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

    for (size_t i = 0; i < depth; ++i) {
      stack.push(reinterpret_cast<uintptr_t>(frame_ptrs[i]));
    }
  }
};

using DefaultPapiBackendSampler =
    PapiBackendSampler<kDefaultMaxStackDepth, kDefaultMapCapacity>;

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLER_PAPI_BACKEND_H_
