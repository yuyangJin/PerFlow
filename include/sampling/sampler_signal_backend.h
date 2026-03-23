// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_SAMPLER_SIGNAL_BACKEND_H_
#define PERFLOW_SAMPLING_SAMPLER_SIGNAL_BACKEND_H_

#include "sampling/sampler.h"

#if !defined(__linux__)
#include <pthread.h>
#include <signal.h>
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
class SamplerSignalBackend : public Sampler<MaxStackDepth, MapCapacity> {
 public:
  using StackType = typename Sampler<MaxStackDepth, MapCapacity>::StackType;
  using MapType = typename Sampler<MaxStackDepth, MapCapacity>::MapType;
  static constexpr int kSampleSignal = SIGUSR1;

  SamplerSignalBackend() noexcept
      : status_(SamplerStatus::kUninitialized),
        sample_count_(0),
        overflow_count_(0),
        running_(false),
        armed_(false),
        in_signal_handler_(0),
        fast_mode_(false),
        torn_down_(false) {}

  ~SamplerSignalBackend() noexcept override { cleanup(); }

  void setMainPthread(pthread_t main_pthread) noexcept { main_pthread_ = main_pthread; }

  SamplerResult initialize(const PmuSamplerConfig& config) noexcept override {
#if !defined(__linux__)
    config_ = config;
    samples_.clear();

    const char* fast_env = std::getenv("PERFLOW_SIGNAL_FAST");
    fast_mode_ = (fast_env && fast_env[0] == '1');

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signalHandlerTrampoline;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(kSampleSignal, &sa, nullptr) < 0) {
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorInitialization;
    }

    active_instance_ = this;
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
#if !defined(__linux__)
    armed_.store(true, std::memory_order_release);
    status_ = SamplerStatus::kRunning;
#endif
    return SamplerResult::kSuccess;
  }

  SamplerResult stop() noexcept override {
#if !defined(__linux__)
    armed_.store(false, std::memory_order_release);
    if (status_ == SamplerStatus::kRunning) status_ = SamplerStatus::kStopped;
#endif
    return SamplerResult::kSuccess;
  }

  void cleanup() noexcept override {
#if !defined(__linux__)
    if (!torn_down_) {
      torn_down_ = true;
      running_.store(false, std::memory_order_relaxed);
      if (sampler_thread_) pthread_join(sampler_thread_, nullptr);
    }
    if (active_instance_ == this) {
      active_instance_ = nullptr;
    }
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
    if (!fast_mode_) {
      size_t depth = capture_call_stack_in_signal(stack, MaxStackDepth);
      if (depth == 0) {
        void* ip = __builtin_return_address(0);
        stack.push(reinterpret_cast<uintptr_t>(ip));
      }
    } else {
      void* ip = __builtin_return_address(0);
      stack.push(reinterpret_cast<uintptr_t>(ip));
    }
    if (!stack.empty()) {
      samples_.increment(stack);
      sample_count_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void handleOverflowWithContext(void* context) noexcept {
    overflow_count_.fetch_add(1, std::memory_order_relaxed);
    StackType stack;
    uintptr_t ip = get_ip_from_ucontext(context);
    if (!fast_mode_) {
      size_t depth = capture_call_stack_in_signal(stack, MaxStackDepth);
      if (depth == 0 && ip) stack.push(ip);
    } else if (ip) {
      stack.push(ip);
    }
    if (!stack.empty()) {
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
  static inline SamplerSignalBackend* active_instance_ = nullptr;

  SamplerStatus status_;
  PmuSamplerConfig config_;
  MapType samples_;
  std::atomic<uint64_t> sample_count_;
  std::atomic<uint64_t> overflow_count_;
  pthread_t main_pthread_{};
  pthread_t sampler_thread_{};
  std::atomic<bool> running_;
  std::atomic<bool> armed_;
  volatile sig_atomic_t in_signal_handler_;
  bool fast_mode_;
  bool torn_down_;

  static void signalHandlerTrampoline(int /*sig*/, siginfo_t* /*info*/, void* context) {
    if (!active_instance_) return;
    auto* self = active_instance_;
    if (self->in_signal_handler_) return;
    self->in_signal_handler_ = 1;
    self->handleOverflowWithContext(context);
    self->in_signal_handler_ = 0;
  }

  static void* threadMain(void* arg) {
    auto* self = static_cast<SamplerSignalBackend*>(arg);
    const int freq = self->config_.frequency > 0
                         ? static_cast<int>(self->config_.frequency)
                         : static_cast<int>(kDefaultSamplingFrequency);
    long long ns = 1000000000LL / freq;
    struct timespec ts;
    ts.tv_sec = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;

    while (self->running_.load(std::memory_order_relaxed)) {
      nanosleep(&ts, nullptr);
      if (!self->armed_.load(std::memory_order_relaxed)) continue;
      pthread_kill(self->main_pthread_, kSampleSignal);
    }
    return nullptr;
  }
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLER_SIGNAL_BACKEND_H_
