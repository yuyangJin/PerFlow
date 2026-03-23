// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_SAMPLER_THREAD_BACKEND_H_
#define PERFLOW_SAMPLING_SAMPLER_THREAD_BACKEND_H_

#include "sampling/sampler.h"

#if defined(__linux__)
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <ctime>
#include <cerrno>
#include <cstdio>
#include <cstring>
#endif

namespace perflow {
namespace sampling {

template <size_t MaxStackDepth = kDefaultMaxStackDepth,
          size_t MapCapacity = kDefaultMapCapacity>
class SamplerThreadBackend : public Sampler<MaxStackDepth, MapCapacity> {
 public:
  using StackType = typename Sampler<MaxStackDepth, MapCapacity>::StackType;
  using MapType = typename Sampler<MaxStackDepth, MapCapacity>::MapType;

  SamplerThreadBackend() noexcept
      : status_(SamplerStatus::kUninitialized),
        sample_count_(0),
        overflow_count_(0),
        main_tid_(0),
        running_(false),
        armed_(false),
        torn_down_(false) {}

  ~SamplerThreadBackend() noexcept override { cleanup(); }

  void setMainTid(pid_t tid) noexcept { main_tid_ = tid; }

  SamplerResult initialize(const PmuSamplerConfig& config) noexcept override {
#if defined(__linux__)
    if (status_ != SamplerStatus::kUninitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }
    if (main_tid_ <= 0) return SamplerResult::kErrorConfiguration;
    config_ = config;
    samples_.clear();

    running_.store(true, std::memory_order_relaxed);
    if (pthread_create(&sampler_thread_, nullptr, threadMain, this) != 0) {
      running_.store(false, std::memory_order_relaxed);
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorInitialization;
    }
    status_ = SamplerStatus::kInitialized;
    return SamplerResult::kSuccess;
#else
    (void)config;
    return SamplerResult::kErrorNotSupported;
#endif
  }

  SamplerResult finalize() noexcept override {
    stop();
    cleanup();
    status_ = SamplerStatus::kStopped;
    return SamplerResult::kSuccess;
  }

  SamplerResult start() noexcept override {
#if defined(__linux__)
    if (status_ != SamplerStatus::kInitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }
    armed_.store(true, std::memory_order_release);
    status_ = SamplerStatus::kRunning;
#endif
    return SamplerResult::kSuccess;
  }

  SamplerResult stop() noexcept override {
#if defined(__linux__)
    armed_.store(false, std::memory_order_release);
    if (status_ == SamplerStatus::kRunning) status_ = SamplerStatus::kStopped;
#endif
    return SamplerResult::kSuccess;
  }

  void cleanup() noexcept override {
#if defined(__linux__)
    if (torn_down_) {
      status_ = SamplerStatus::kUninitialized;
      return;
    }
    torn_down_ = true;
    running_.store(false, std::memory_order_relaxed);
    if (sampler_thread_) pthread_join(sampler_thread_, nullptr);
#endif
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
    uintptr_t pc = readThreadIp(main_tid_);
    if (pc) {
      stack.push(pc);
      samples_.increment(stack);
      sample_count_.fetch_add(1, std::memory_order_relaxed);
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
    config_ = config;
    return SamplerResult::kSuccess;
  }

 private:
  SamplerStatus status_;
  PmuSamplerConfig config_;
  MapType samples_;
  std::atomic<uint64_t> sample_count_;
  std::atomic<uint64_t> overflow_count_;
  pid_t main_tid_;
  pthread_t sampler_thread_{};
  std::atomic<bool> running_;
  std::atomic<bool> armed_;
  bool torn_down_;

  static uintptr_t readThreadIp(pid_t tid) {
#if defined(__linux__)
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/self/task/%d/syscall", tid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    if (buf[0] == 'r') return 0;
    char* sp = std::strrchr(buf, ' ');
    if (!sp) return 0;
    uintptr_t pc = 0;
    if (std::sscanf(sp + 1, "0x%lx", &pc) != 1) {
      std::sscanf(sp + 1, "%lx", &pc);
    }
    return pc;
#else
    (void)tid;
    return 0;
#endif
  }

  static void* threadMain(void* arg) {
    auto* self = static_cast<SamplerThreadBackend*>(arg);
    const int freq = self->config_.frequency > 0
                         ? static_cast<int>(self->config_.frequency)
                         : static_cast<int>(kDefaultSamplingFrequency);
    long long ns = 1000000000LL / freq;
    struct timespec ts;
    ts.tv_sec = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;

    while (self->running_.load(std::memory_order_relaxed)) {
      clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
      if (!self->armed_.load(std::memory_order_relaxed)) continue;
      self->handleOverflow();
    }
    return nullptr;
  }
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLER_THREAD_BACKEND_H_
