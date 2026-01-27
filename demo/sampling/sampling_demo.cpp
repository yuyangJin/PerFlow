// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

/**
 * @file sampling_demo.cpp
 * @brief Demonstration of PAPI-based performance sampling
 *
 * This demo shows how to use PAPI for performance sampling in a simple
 * sequential program. It serves as a reference implementation for
 * understanding the sampling approach.
 *
 * Compile:
 *   g++ -o sampling_demo sampling_demo.cpp -lpapi -ldl
 *
 * Run:
 *   ./sampling_demo
 */

#include <papi.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <vector>

// Number of events to monitor
constexpr int NUM_EVENTS = 3;

// Global state for signal handler
static int event_set = PAPI_NULL;
static volatile long sample_count = 0;
static std::vector<long long> sample_values;

/**
 * @brief PAPI overflow handler
 *
 * This function is called when a hardware counter overflows.
 * It must be signal-safe.
 */
void overflow_handler(int EventSet, void* address, long long overflow_vector,
                      void* context) {
  (void)EventSet;
  (void)address;
  (void)overflow_vector;
  (void)context;

  long long values[NUM_EVENTS];
  PAPI_read(event_set, values);

  // Store sample (note: not signal-safe in production!)
  for (int i = 0; i < NUM_EVENTS; ++i) {
    sample_values.push_back(values[i]);
  }
  sample_count++;
}

/**
 * @brief Initialize PAPI and setup event set
 */
int initialize_papi() {
  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT) {
    std::cerr << "PAPI library init error: " << PAPI_strerror(ret) << std::endl;
    return -1;
  }

  // Create event set
  ret = PAPI_create_eventset(&event_set);
  if (ret != PAPI_OK) {
    std::cerr << "PAPI create eventset error: " << PAPI_strerror(ret) << std::endl;
    return -1;
  }

  // Add events
  int events[NUM_EVENTS] = {PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_L1_DCM};
  for (int i = 0; i < NUM_EVENTS; ++i) {
    ret = PAPI_add_event(event_set, events[i]);
    if (ret != PAPI_OK) {
      std::cerr << "Warning: Failed to add event " << i << ": "
                << PAPI_strerror(ret) << std::endl;
    }
  }

  return 0;
}

/**
 * @brief Setup overflow handler
 */
int setup_overflow(int threshold) {
  int ret = PAPI_overflow(event_set, PAPI_TOT_CYC, threshold, 0,
                          overflow_handler);
  if (ret != PAPI_OK) {
    std::cerr << "PAPI overflow error: " << PAPI_strerror(ret) << std::endl;
    return -1;
  }
  return 0;
}

/**
 * @brief Workload: simple computation
 */
void do_work() {
  const int N = 1000;
  std::vector<double> A(N * N);
  std::vector<double> B(N * N);
  std::vector<double> C(N * N);

  // Initialize
  for (int i = 0; i < N * N; ++i) {
    A[i] = static_cast<double>(i) / N;
    B[i] = static_cast<double>(N * N - i) / N;
  }

  // Simple matrix-vector multiply
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      double sum = 0.0;
      for (int k = 0; k < N; ++k) {
        sum += A[i * N + k] * B[k * N + j];
      }
      C[i * N + j] = sum;
    }
  }

  // Use result
  double total = 0.0;
  for (int i = 0; i < N * N; ++i) {
    total += C[i];
  }
  std::cout << "Computation result: " << total << std::endl;
}

int main() {
  std::cout << "=== PAPI Sampling Demo ===" << std::endl;

  // Initialize PAPI
  if (initialize_papi() != 0) {
    return 1;
  }
  std::cout << "PAPI initialized successfully" << std::endl;

  // Setup overflow handler (sample every 1 million cycles)
  if (setup_overflow(1000000) != 0) {
    return 1;
  }
  std::cout << "Overflow handler set (threshold: 1M cycles)" << std::endl;

  // Start counting
  int ret = PAPI_start(event_set);
  if (ret != PAPI_OK) {
    std::cerr << "PAPI start error: " << PAPI_strerror(ret) << std::endl;
    return 1;
  }

  auto start = std::chrono::high_resolution_clock::now();

  // Do work
  do_work();

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      end - start)
                      .count();

  // Stop counting
  long long values[NUM_EVENTS];
  ret = PAPI_stop(event_set, values);
  if (ret != PAPI_OK) {
    std::cerr << "PAPI stop error: " << PAPI_strerror(ret) << std::endl;
    return 1;
  }

  // Report results
  std::cout << "\n=== Results ===" << std::endl;
  std::cout << "Execution time: " << duration << " ms" << std::endl;
  std::cout << "Sample count: " << sample_count << std::endl;
  std::cout << "Final counter values:" << std::endl;
  std::cout << "  Total Cycles: " << values[0] << std::endl;
  std::cout << "  Total Instructions: " << values[1] << std::endl;
  std::cout << "  L1 D-Cache Misses: " << values[2] << std::endl;

  if (values[1] > 0) {
    double ipc = static_cast<double>(values[1]) / values[0];
    std::cout << "  IPC: " << ipc << std::endl;
  }

  // Cleanup
  PAPI_cleanup_eventset(event_set);
  PAPI_destroy_eventset(&event_set);

  std::cout << "\n=== Demo Complete ===" << std::endl;
  return 0;
}
