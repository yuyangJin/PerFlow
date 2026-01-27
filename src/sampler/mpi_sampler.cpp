// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

/**
 * @file mpi_sampler.cpp
 * @brief MPI Performance Sampler using PAPI library
 *
 * This module implements a performance sampler for MPI programs using the
 * PAPI (Performance API) library. It collects program snapshots when hardware
 * performance counter data overflows.
 *
 * Key Features:
 * - Configurable sampling frequency (default: 1000 Hz)
 * - Tracks multiple hardware events (PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_L1_DCM)
 * - Thread-safe operations
 * - Signal-safe overflow handling
 * - Supports compressed and uncompressed output formats
 */

#include "sampling/sampler.h"
#include "sampling/call_stack.h"
#include "sampling/data_collection.h"

#include <papi.h>
#include <mpi.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

namespace perflow {
namespace sampling {

// Forward declaration
class MPISampler;

// Global pointer for signal handler access
static MPISampler* g_sampler_instance = nullptr;

/**
 * @brief MPI Performance Sampler implementation using PAPI
 */
class MPISampler : public Sampler {
 public:
  MPISampler();
  ~MPISampler() override;

  int Initialize(const SamplerConfig& config) override;
  int Start() override;
  int Stop() override;
  int Finalize() override;
  const std::vector<Sample>& GetSamples() const override;
  int WriteOutput() override;
  size_t GetSampleCount() const override;
  bool IsActive() const override;

  // Called from overflow handler (signal-safe)
  void HandleOverflow(int event_set, void* address, long long* overflow_values,
                      void* context);

 private:
  int InitializePAPI();
  int SetupEventSet();
  int SetupOverflowHandler();
  static void OverflowHandler(int event_set, void* address,
                              long long overflow_vector,
                              void* context);

  SamplerConfig config_;
  DataCollection data_collection_;
  int event_set_;
  int mpi_rank_;
  int mpi_size_;
  std::atomic<bool> is_active_;
  std::atomic<bool> is_initialized_;

  // PAPI event codes
  static constexpr int kNumEvents = 3;
  int events_[kNumEvents];
  int overflow_event_;
  int overflow_threshold_;

  // Thread-local storage for call stack capture
  static thread_local void* stack_buffer_[kMaxStackDepth];
};

// Thread-local stack buffer
thread_local void* MPISampler::stack_buffer_[kMaxStackDepth];

MPISampler::MPISampler()
    : event_set_(PAPI_NULL),
      mpi_rank_(0),
      mpi_size_(1),
      is_active_(false),
      is_initialized_(false),
      overflow_event_(PAPI_TOT_CYC),
      overflow_threshold_(1000000) {
  // Default events: Total Cycles, Total Instructions, L1 Data Cache Misses
  events_[0] = PAPI_TOT_CYC;
  events_[1] = PAPI_TOT_INS;
  events_[2] = PAPI_L1_DCM;
}

MPISampler::~MPISampler() {
  if (is_active_) {
    Stop();
  }
  if (is_initialized_) {
    Finalize();
  }
}

int MPISampler::Initialize(const SamplerConfig& config) {
  config_ = config;

  // Get MPI rank and size
  int mpi_initialized = 0;
  MPI_Initialized(&mpi_initialized);
  if (mpi_initialized) {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size_);
  }

  // Initialize PAPI
  int ret = InitializePAPI();
  if (ret != PAPI_OK) {
    std::cerr << "Rank " << mpi_rank_ << ": Failed to initialize PAPI: "
              << PAPI_strerror(ret) << std::endl;
    return -1;
  }

  // Setup event set
  ret = SetupEventSet();
  if (ret != PAPI_OK) {
    std::cerr << "Rank " << mpi_rank_ << ": Failed to setup event set: "
              << PAPI_strerror(ret) << std::endl;
    return -2;
  }

  // Calculate overflow threshold based on sampling frequency
  // Assuming ~2GHz processor: threshold = cycles_per_second / freq
  overflow_threshold_ = 2000000000 / config_.sampling_frequency;

  // Initialize data collection
  OutputFormat format = config_.compress_output ? OutputFormat::kCompressed
                                                 : OutputFormat::kText;
  ret = data_collection_.Initialize(config_.output_path, format, mpi_rank_);
  if (ret != 0) {
    std::cerr << "Rank " << mpi_rank_ << ": Failed to initialize data collection"
              << std::endl;
    return -3;
  }

  // Set global instance for signal handler
  g_sampler_instance = this;

  is_initialized_ = true;
  return 0;
}

int MPISampler::InitializePAPI() {
  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT) {
    if (ret > 0) {
      std::cerr << "PAPI library version mismatch" << std::endl;
    }
    return ret;
  }

  // Initialize thread support
  ret = PAPI_thread_init((unsigned long (*)(void))pthread_self);
  if (ret != PAPI_OK) {
    std::cerr << "Failed to initialize PAPI thread support: "
              << PAPI_strerror(ret) << std::endl;
    return ret;
  }

  return PAPI_OK;
}

int MPISampler::SetupEventSet() {
  int ret;

  // Create event set
  event_set_ = PAPI_NULL;
  ret = PAPI_create_eventset(&event_set_);
  if (ret != PAPI_OK) {
    return ret;
  }

  // Add events to the set
  for (int i = 0; i < kNumEvents; ++i) {
    ret = PAPI_add_event(event_set_, events_[i]);
    if (ret != PAPI_OK) {
      std::cerr << "Warning: Failed to add event " << i << ": "
                << PAPI_strerror(ret) << std::endl;
      // Try to continue with remaining events
    }
  }

  return PAPI_OK;
}

int MPISampler::SetupOverflowHandler() {
  int ret = PAPI_overflow(event_set_, overflow_event_, overflow_threshold_,
                          0, OverflowHandler);
  if (ret != PAPI_OK) {
    std::cerr << "Failed to set overflow handler: " << PAPI_strerror(ret)
              << std::endl;
    return ret;
  }
  return PAPI_OK;
}

void MPISampler::OverflowHandler(int event_set, void* address,
                                  long long overflow_vector,
                                  void* context) {
  if (g_sampler_instance == nullptr) {
    return;
  }

  // Read current counter values
  long long values[kNumEvents];
  PAPI_read(event_set, values);

  // Call instance handler
  g_sampler_instance->HandleOverflow(event_set, address, values, context);
}

void MPISampler::HandleOverflow(int event_set, void* address,
                                 long long* overflow_values, void* context) {
  if (!is_active_) {
    return;
  }

  // Get timestamp (signal-safe using clock_gettime)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t timestamp = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
                       static_cast<uint64_t>(ts.tv_nsec);

  // Capture call stack (signal-safe)
  int stack_depth = 0;
  if (config_.enable_call_stack) {
    stack_depth = CaptureCallStackRaw(stack_buffer_, kMaxStackDepth);
  }

  // Convert values
  int64_t event_values[kNumEvents];
  for (int i = 0; i < kNumEvents; ++i) {
    event_values[i] = static_cast<int64_t>(overflow_values[i]);
  }

  // Get thread ID
  int thread_id = static_cast<int>(pthread_self());

  // Add sample using signal-safe method
  data_collection_.AddSampleSignalSafe(timestamp, event_values, kNumEvents,
                                       stack_buffer_, stack_depth, thread_id);
}

int MPISampler::Start() {
  if (!is_initialized_) {
    std::cerr << "Rank " << mpi_rank_ << ": Sampler not initialized" << std::endl;
    return -1;
  }

  if (is_active_) {
    return 0;  // Already started
  }

  // Setup overflow handler
  int ret = SetupOverflowHandler();
  if (ret != PAPI_OK) {
    return -2;
  }

  // Start counting
  ret = PAPI_start(event_set_);
  if (ret != PAPI_OK) {
    std::cerr << "Rank " << mpi_rank_ << ": Failed to start PAPI: "
              << PAPI_strerror(ret) << std::endl;
    return -3;
  }

  is_active_ = true;
  return 0;
}

int MPISampler::Stop() {
  if (!is_active_) {
    return 0;  // Already stopped
  }

  is_active_ = false;

  // Stop counting
  long long values[kNumEvents];
  int ret = PAPI_stop(event_set_, values);
  if (ret != PAPI_OK) {
    std::cerr << "Rank " << mpi_rank_ << ": Failed to stop PAPI: "
              << PAPI_strerror(ret) << std::endl;
    return -1;
  }

  // Flush any pending signal-safe samples
  data_collection_.FlushSignalSafeSamples();

  return 0;
}

int MPISampler::Finalize() {
  if (!is_initialized_) {
    return 0;
  }

  // Cleanup event set
  if (event_set_ != PAPI_NULL) {
    PAPI_cleanup_eventset(event_set_);
    PAPI_destroy_eventset(&event_set_);
    event_set_ = PAPI_NULL;
  }

  // Clear global instance
  g_sampler_instance = nullptr;

  is_initialized_ = false;
  return 0;
}

const std::vector<Sample>& MPISampler::GetSamples() const {
  return data_collection_.GetSamples();
}

int MPISampler::WriteOutput() {
  return data_collection_.WriteOutput();
}

size_t MPISampler::GetSampleCount() const {
  return data_collection_.GetSampleCount();
}

bool MPISampler::IsActive() const {
  return is_active_;
}

// Global sampler instance for constructor/destructor hooks
static MPISampler* g_global_sampler = nullptr;
static SamplerConfig g_default_config;

/**
 * @brief Constructor hook - called when shared library is loaded
 */
__attribute__((constructor))
static void sampler_constructor() {
  // Check if sampling is enabled via environment variable
  const char* enable_env = std::getenv("PERFLOW_ENABLE_SAMPLING");
  if (enable_env == nullptr || std::strcmp(enable_env, "1") != 0) {
    return;
  }

  // Read configuration from environment
  const char* freq_env = std::getenv("PERFLOW_SAMPLING_FREQ");
  if (freq_env != nullptr) {
    g_default_config.sampling_frequency = std::atoi(freq_env);
    if (g_default_config.sampling_frequency <= 0) {
      g_default_config.sampling_frequency = 1000;
    }
  }

  const char* output_env = std::getenv("PERFLOW_OUTPUT_PATH");
  if (output_env != nullptr) {
    g_default_config.output_path = output_env;
  }

  const char* compress_env = std::getenv("PERFLOW_COMPRESS");
  if (compress_env != nullptr && std::strcmp(compress_env, "1") == 0) {
    g_default_config.compress_output = true;
  }

  const char* callstack_env = std::getenv("PERFLOW_CALLSTACK");
  if (callstack_env != nullptr && std::strcmp(callstack_env, "0") == 0) {
    g_default_config.enable_call_stack = false;
  }

  // Create and initialize sampler
  g_global_sampler = new MPISampler();
  int ret = g_global_sampler->Initialize(g_default_config);
  if (ret != 0) {
    std::cerr << "Failed to initialize global sampler" << std::endl;
    delete g_global_sampler;
    g_global_sampler = nullptr;
    return;
  }

  // Start sampling
  ret = g_global_sampler->Start();
  if (ret != 0) {
    std::cerr << "Failed to start global sampler" << std::endl;
    g_global_sampler->Finalize();
    delete g_global_sampler;
    g_global_sampler = nullptr;
    return;
  }
}

/**
 * @brief Destructor hook - called when shared library is unloaded
 */
__attribute__((destructor))
static void sampler_destructor() {
  if (g_global_sampler == nullptr) {
    return;
  }

  // Stop sampling
  g_global_sampler->Stop();

  // Write output
  g_global_sampler->WriteOutput();

  // Get MPI rank for output message
  int mpi_initialized = 0;
  int rank = 0;
  MPI_Initialized(&mpi_initialized);
  if (mpi_initialized) {
    int mpi_finalized = 0;
    MPI_Finalized(&mpi_finalized);
    if (!mpi_finalized) {
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    }
  }

  std::cout << "Rank " << rank << ": Collected "
            << g_global_sampler->GetSampleCount() << " samples" << std::endl;

  // Cleanup
  g_global_sampler->Finalize();
  delete g_global_sampler;
  g_global_sampler = nullptr;
}

}  // namespace sampling
}  // namespace perflow

// C API for external use
extern "C" {

/**
 * @brief Create a new MPI sampler instance
 * @return Pointer to new sampler instance
 */
void* perflow_sampler_create() {
  return new perflow::sampling::MPISampler();
}

/**
 * @brief Initialize the sampler
 * @param sampler Sampler instance
 * @param frequency Sampling frequency in Hz
 * @param output_path Output file path prefix
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_init(void* sampler, int frequency, const char* output_path) {
  if (sampler == nullptr) {
    return -1;
  }

  perflow::sampling::SamplerConfig config;
  config.sampling_frequency = frequency;
  if (output_path != nullptr) {
    config.output_path = output_path;
  }

  auto* s = static_cast<perflow::sampling::MPISampler*>(sampler);
  return s->Initialize(config);
}

/**
 * @brief Start sampling
 * @param sampler Sampler instance
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_start(void* sampler) {
  if (sampler == nullptr) {
    return -1;
  }
  auto* s = static_cast<perflow::sampling::MPISampler*>(sampler);
  return s->Start();
}

/**
 * @brief Stop sampling
 * @param sampler Sampler instance
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_stop(void* sampler) {
  if (sampler == nullptr) {
    return -1;
  }
  auto* s = static_cast<perflow::sampling::MPISampler*>(sampler);
  return s->Stop();
}

/**
 * @brief Write sampler output
 * @param sampler Sampler instance
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_write_output(void* sampler) {
  if (sampler == nullptr) {
    return -1;
  }
  auto* s = static_cast<perflow::sampling::MPISampler*>(sampler);
  return s->WriteOutput();
}

/**
 * @brief Finalize and destroy the sampler
 * @param sampler Sampler instance
 */
void perflow_sampler_destroy(void* sampler) {
  if (sampler == nullptr) {
    return;
  }
  auto* s = static_cast<perflow::sampling::MPISampler*>(sampler);
  s->Finalize();
  delete s;
}

/**
 * @brief Get the number of samples collected
 * @param sampler Sampler instance
 * @return Number of samples
 */
size_t perflow_sampler_get_sample_count(void* sampler) {
  if (sampler == nullptr) {
    return 0;
  }
  auto* s = static_cast<perflow::sampling::MPISampler*>(sampler);
  return s->GetSampleCount();
}

}  // extern "C"
