// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_SAMPLER_PERF_PMU_BACKEND_H_
#define PERFLOW_SAMPLING_SAMPLER_PERF_PMU_BACKEND_H_

#include "sampling/sampler.h"

#if defined(__linux__)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

namespace perflow {
namespace sampling {

template <size_t MaxStackDepth = kDefaultMaxStackDepth,
          size_t MapCapacity = kDefaultMapCapacity>
class SamplerPerfPmuBackend : public Sampler<MaxStackDepth, MapCapacity> {
 public:
  using StackType = typename Sampler<MaxStackDepth, MapCapacity>::StackType;
  using MapType = typename Sampler<MaxStackDepth, MapCapacity>::MapType;

  SamplerPerfPmuBackend() noexcept
      : status_(SamplerStatus::kUninitialized),
        sample_count_(0),
        overflow_count_(0),
        perf_fd_(-1),
        main_tid_(0),
        fast_mode_(false) {}

  ~SamplerPerfPmuBackend() noexcept override { cleanup(); }

  void setMainTid(pid_t tid) noexcept { main_tid_ = tid; }

  SamplerResult initialize(const PmuSamplerConfig& config) noexcept override {
#if defined(__linux__)
    if (status_ != SamplerStatus::kUninitialized &&
        status_ != SamplerStatus::kStopped) {
      return SamplerResult::kErrorInitialization;
    }
    if (main_tid_ <= 0) {
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorConfiguration;
    }
    config_ = config;
    samples_.clear();

    const char* fast_env = std::getenv("PERFLOW_PMU_FAST");
    fast_mode_ = (fast_env && fast_env[0] == '1');

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = overflowHandlerTrampoline;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGIO, &sa, nullptr) < 0) {
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorInitialization;
    }

    long long period = computeSamplePeriod();
    struct perf_event_attr pe;
    std::memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.sample_period = static_cast<uint64_t>(period);
    pe.sample_type = PERF_SAMPLE_IP;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.wakeup_events = 1;

    perf_fd_ = static_cast<int>(perfEventOpenSys(&pe, 0, -1, -1, 0));
    if (perf_fd_ < 0) {
      status_ = SamplerStatus::kError;
      return SamplerResult::kErrorPermission;
    }

    struct f_owner_ex owner;
    owner.type = F_OWNER_TID;
    owner.pid = main_tid_;
    fcntl(perf_fd_, F_SETOWN_EX, &owner);
    fcntl(perf_fd_, F_SETFL, O_ASYNC);
    fcntl(perf_fd_, F_SETSIG, SIGIO);

    active_instance_ = this;
    status_ = SamplerStatus::kInitialized;
    return SamplerResult::kSuccess;
#else
    (void)config;
    status_ = SamplerStatus::kError;
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
    if (perf_fd_ < 0) return SamplerResult::kErrorInitialization;
    ioctl(perf_fd_, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_fd_, PERF_EVENT_IOC_REFRESH, 1);
    status_ = SamplerStatus::kRunning;
    return SamplerResult::kSuccess;
#else
    return SamplerResult::kErrorNotSupported;
#endif
  }

  SamplerResult stop() noexcept override {
#if defined(__linux__)
    if (status_ != SamplerStatus::kRunning) return SamplerResult::kSuccess;
    if (perf_fd_ >= 0) ioctl(perf_fd_, PERF_EVENT_IOC_DISABLE, 0);
    status_ = SamplerStatus::kStopped;
#endif
    return SamplerResult::kSuccess;
  }

  void cleanup() noexcept override {
#if defined(__linux__)
    if (perf_fd_ >= 0) {
      close(perf_fd_);
      perf_fd_ = -1;
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
  static constexpr double kDefaultCpuFreqGHz = 2.6;
  static inline SamplerPerfPmuBackend* active_instance_ = nullptr;

  SamplerStatus status_;
  PmuSamplerConfig config_;
  MapType samples_;
  std::atomic<uint64_t> sample_count_;
  std::atomic<uint64_t> overflow_count_;
  int perf_fd_;
  pid_t main_tid_;
  bool fast_mode_;
  volatile sig_atomic_t in_signal_handler_{0};

#if defined(__linux__)
  static long perfEventOpenSys(struct perf_event_attr* a, pid_t pid, int cpu,
                               int gfd, unsigned long fl) {
#ifndef __NR_perf_event_open
#if defined(__x86_64__)
#define __NR_perf_event_open 298
#elif defined(__aarch64__)
#define __NR_perf_event_open 241
#else
#define __NR_perf_event_open 364
#endif
#endif
    return syscall(__NR_perf_event_open, a, pid, cpu, gfd, fl);
  }

  long long computeSamplePeriod() const {
    const char* p_env = std::getenv("PERFLOW_PMU_PERIOD");
    if (p_env) {
      long long p = std::atoll(p_env);
      if (p > 0) return p;
    }
    const char* c_env = std::getenv("PERFLOW_CPU_FREQ_GHZ");
    double ghz = kDefaultCpuFreqGHz;
    if (c_env) {
      double v = std::atof(c_env);
      if (v > 0.1) ghz = v;
    }
    double hz = ghz * 1e9;
    const int freq = config_.frequency > 0 ? static_cast<int>(config_.frequency)
                                           : static_cast<int>(kDefaultSamplingFrequency);
    return static_cast<long long>(hz / static_cast<double>(freq));
  }

  static void overflowHandlerTrampoline(int /*sig*/, siginfo_t* info,
                                        void* context) {
    if (!active_instance_) return;
    auto* self = active_instance_;
    if (self->in_signal_handler_) {
      if (info && info->si_fd == self->perf_fd_) {
        ioctl(self->perf_fd_, PERF_EVENT_IOC_REFRESH, 1);
      }
      return;
    }
    self->in_signal_handler_ = 1;
    self->handleOverflowWithContext(context);
    self->in_signal_handler_ = 0;
    if (self->perf_fd_ >= 0) {
      ioctl(self->perf_fd_, PERF_EVENT_IOC_REFRESH, 1);
    }
  }
#endif
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLER_PERF_PMU_BACKEND_H_
