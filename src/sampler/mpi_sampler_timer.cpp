// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_sampler_timer.cpp
/// @brief MPI performance sampler: hooks MPI init/finalize and dispatches sampling backends
///
/// Backends (implementation under include/sampling/):
/// - Linux PMU: perf_event_open + SIGIO + libunwind (default on Linux)
/// - Linux thread: poll /proc for IP (set PERFLOW_SAMPLER_BACKEND=thread)
/// - Non-Linux: pthread_kill + SIGUSR1 + libunwind
///
/// Select Linux backend at runtime:
///   export PERFLOW_SAMPLER_BACKEND=pmu    # default
///   export PERFLOW_SAMPLER_BACKEND=thread # signal-free /proc polling

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>

#if defined(__linux__)
#include <sys/syscall.h>
#include <strings.h>
#endif

#if defined(__APPLE__)
#include <pthread.h>
#endif

#include "sampling/sampler.h"
#include "sampling/sampler_perf_pmu_backend.h"
#include "sampling/sampler_signal_backend.h"
#include "sampling/sampler_thread_backend.h"
#include "sampling/library_map.h"
#include "sampling/data_export.h"
#include "sampling/mpi_init_setup.h"

using namespace perflow::sampling;

// ============================================================================
// Shared configuration
// ============================================================================

constexpr int kDefaultSamplingFreq = 1000;
constexpr size_t kMaxCallStackDepth = 100;
constexpr size_t kSampleMapCapacity = 10000;

extern int g_mpi_rank;

static bool g_module_initialized = false;
static int g_sampling_frequency = kDefaultSamplingFreq;

using SampleCallStack = CallStack<kMaxCallStackDepth>;
using SampleHashMap = StaticHashMap<SampleCallStack, uint64_t, kSampleMapCapacity>;
using SamplerBase = Sampler<kMaxCallStackDepth, kSampleMapCapacity>;

#if defined(__linux__)
using PerfPmuBackend = SamplerPerfPmuBackend<kMaxCallStackDepth, kSampleMapCapacity>;
using ThreadBackend = SamplerThreadBackend<kMaxCallStackDepth, kSampleMapCapacity>;
#endif
#if !defined(__linux__)
using SignalBackend = SamplerSignalBackend<kMaxCallStackDepth, kSampleMapCapacity>;
#endif

enum class ActiveBackend {
  kNone = 0,
  kPmu,
  kThread,
  kSignal,
};

static std::unique_ptr<SamplerBase> g_backend_sampler = nullptr;
static std::unique_ptr<LibraryMap> g_library_map = nullptr;
static pid_t g_main_tid = 0;
static pthread_t g_main_pthread;

static ActiveBackend g_active_backend = ActiveBackend::kNone;

static uint64_t get_current_thread_id() {
#if defined(__APPLE__)
  uint64_t tid = 0;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#elif defined(__linux__)
  return static_cast<uint64_t>(syscall(SYS_gettid));
#else
  return 0;
#endif
}

#if defined(__linux__)
static bool env_wants_thread_backend() {
  const char* be = std::getenv("PERFLOW_SAMPLER_BACKEND");
  if (!be || !be[0]) return false;
  return strcasecmp(be, "thread") == 0;
}

static void warn_unknown_backend_env(const char* be) {
  if (!be || !be[0]) return;
  if (strcasecmp(be, "thread") == 0 || strcasecmp(be, "pmu") == 0 ||
      strcasecmp(be, "hw") == 0)
    return;
  std::fprintf(stderr,
               "[MPI Sampler] Unknown PERFLOW_SAMPLER_BACKEND=%s; using pmu\n",
               be);
}
#endif

static bool setup_active_backend() {
  PmuSamplerConfig cfg;
  cfg.frequency = static_cast<uint64_t>(g_sampling_frequency);
  std::strcpy(cfg.output_filename, "perflow_mpi_rank");
#if defined(__linux__)
  warn_unknown_backend_env(std::getenv("PERFLOW_SAMPLER_BACKEND"));
  if (env_wants_thread_backend()) {
    auto backend = std::make_unique<ThreadBackend>();
    backend->setMainTid(g_main_tid);
    if (backend->initialize(cfg) == SamplerResult::kSuccess) {
      g_backend_sampler = std::move(backend);
      g_active_backend = ActiveBackend::kThread;
      return true;
    }
    return false;
  }
  auto backend = std::make_unique<PerfPmuBackend>();
  backend->setMainTid(g_main_tid);
  if (backend->initialize(cfg) == SamplerResult::kSuccess) {
    g_backend_sampler = std::move(backend);
    g_active_backend = ActiveBackend::kPmu;
    return true;
  }
  return false;
#else
  auto backend = std::make_unique<SignalBackend>();
  backend->setMainPthread(g_main_pthread);
  if (backend->initialize(cfg) == SamplerResult::kSuccess) {
    g_backend_sampler = std::move(backend);
    g_active_backend = ActiveBackend::kSignal;
    return true;
  }
  return false;
#endif
}

static void arm_backend() {
  if (!g_backend_sampler) return;
  g_backend_sampler->start();
}

static void teardown_backend() {
  if (g_backend_sampler) {
    g_backend_sampler->stop();
    g_backend_sampler->cleanup();
  }
  g_active_backend = ActiveBackend::kNone;
}

// ############################################################################
// Initialization / Arm / Finalization
// ############################################################################

static void initialize_sampler() __attribute__((constructor));
static void finalize_sampler() __attribute__((destructor));
extern "C" void perflow_arm_timer();
extern "C" void perflow_disarm_sampler();

static struct timespec g_arm_time = {0, 0};
static struct timespec g_disarm_time = {0, 0};

static void initialize_sampler() {
  if (g_module_initialized) return;
  g_module_initialized = true;
  g_mpi_rank = -1;
  g_main_pthread = pthread_self();
  g_main_tid = static_cast<pid_t>(get_current_thread_id());

  const char* freq_env = std::getenv("PERFLOW_SAMPLING_FREQ");
  if (freq_env) {
    g_sampling_frequency = std::atoi(freq_env);
    if (g_sampling_frequency <= 0) g_sampling_frequency = kDefaultSamplingFreq;
  }

#if defined(__linux__)
  if (env_wants_thread_backend()) {
    std::fprintf(stderr,
                 "[MPI Sampler Thread] Init freq=%d Hz (thread, signal-free)\n",
                 g_sampling_frequency);
  } else {
    std::fprintf(stderr,
                 "[MPI Sampler PMU] Init freq=%d Hz (perf_event + SIGIO)\n",
                 g_sampling_frequency);
  }
#else
  std::fprintf(stderr,
               "[MPI Sampler Signal] Init freq=%d Hz (SIGUSR1 + libunwind)\n",
               g_sampling_frequency);
#endif

  g_library_map = std::make_unique<LibraryMap>();
  if (!g_library_map->parse_current_process()) {
    std::fprintf(stderr, "[MPI Sampler] Warning: failed to parse library map\n");
  } else {
    std::fprintf(stderr, "[MPI Sampler] Captured %zu library mappings\n",
                 g_library_map->size());
  }

  if (!setup_active_backend()) {
    std::fprintf(stderr, "[MPI Sampler] Backend setup failed\n");
    g_module_initialized = false;
    return;
  }

  std::fprintf(stderr, "[MPI Sampler] Init OK, will arm after MPI_Init\n");
}

extern "C" void perflow_arm_timer() {
  clock_gettime(CLOCK_MONOTONIC, &g_arm_time);
  arm_backend();
}

extern "C" void perflow_disarm_sampler() {
  clock_gettime(CLOCK_MONOTONIC, &g_disarm_time);
  teardown_backend();
  double elapsed = (g_disarm_time.tv_sec - g_arm_time.tv_sec) +
                   (g_disarm_time.tv_nsec - g_arm_time.tv_nsec) * 1e-9;
  std::fprintf(stderr,
               "[MPI Sampler] Disarmed before MPI_Finalize "
               "(sampled region: %.3f s)\n",
               elapsed);
}

static void finalize_sampler() {
  if (!g_module_initialized) return;
  std::fprintf(stderr, "[MPI Sampler] Finalizing\n");

  teardown_backend();

  if (g_mpi_rank < 0)
    std::fprintf(stderr,
                 "[MPI Sampler] Warning: MPI rank not captured, using -1\n");

  if (g_backend_sampler) {
    const auto& samples = g_backend_sampler->samples();
    if (g_mpi_rank == 0)
      std::fprintf(stderr, "[MPI Sampler] Rank %d - %zu unique call stacks\n",
                   g_mpi_rank, samples.size());

    char output_dir[256];
    const char* dir_env = std::getenv("PERFLOW_OUTPUT_DIR");
    if (dir_env) {
      std::strncpy(output_dir, dir_env, sizeof(output_dir) - 1);
      output_dir[sizeof(output_dir) - 1] = '\0';
    } else {
      std::strcpy(output_dir, "/tmp");
    }

    char fname[256];
    std::snprintf(fname, sizeof(fname), "perflow_mpi_rank_%d", g_mpi_rank);

    DataExporter exporter(output_dir, fname, false);
    DataResult res = exporter.exportData(samples);
    if (res == DataResult::kSuccess) {
      if (g_mpi_rank == 0)
        std::fprintf(stderr, "[MPI Sampler] Rank %d - Data -> %s\n", g_mpi_rank,
                     exporter.filepath());
    } else {
      std::fprintf(stderr, "[MPI Sampler] Rank %d - export failed\n",
                   g_mpi_rank);
    }

    if (g_mpi_rank == 0) {
      DataResult tr = exporter.exportDataText(samples);
      if (tr == DataResult::kSuccess)
        std::fprintf(stderr, "[MPI Sampler] Rank 0 - text data exported\n");
    }

    if (g_library_map && !g_library_map->empty()) {
      LibraryMapExporter mexp(output_dir, fname);
      DataResult mr =
          mexp.exportMap(*g_library_map, static_cast<uint32_t>(g_mpi_rank));
      if (mr == DataResult::kSuccess && g_mpi_rank == 0)
        std::fprintf(stderr, "[MPI Sampler] Rank 0 - libmap -> %s\n",
                     mexp.filepath());

      if (g_mpi_rank == 0)
        mexp.exportMapText(*g_library_map, static_cast<uint32_t>(g_mpi_rank));
    }
  }

  g_backend_sampler.reset();
  g_library_map.reset();

  if (g_mpi_rank == 0) std::fprintf(stderr, "[MPI Sampler] Done\n");
}
