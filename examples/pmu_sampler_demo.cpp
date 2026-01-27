// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file pmu_sampler_demo.cpp
/// @brief Demonstration of the PmuSampler usage for performance profiling
///
/// This example shows how to:
/// 1. Configure and initialize the PMU sampler
/// 2. Start/stop sampling during workload execution
/// 3. Simulate overflow events (in real usage, these come from PMU hardware)
/// 4. Access and analyze collected samples
/// 5. Export data to files

#include <cstdio>
#include <cstdlib>

#include "sampling/sampling.h"

using namespace perflow::sampling;

/// Simulated workload function - represents a compute-intensive operation
void computeIntensive(int iterations) {
  volatile double result = 0.0;
  for (int i = 0; i < iterations; ++i) {
    result += static_cast<double>(i) * 0.001;
  }
  (void)result;
}

/// Simulated workload function - represents memory-intensive operation
void memoryIntensive(int size) {
  volatile int* buffer = new int[size];
  for (int i = 0; i < size; ++i) {
    buffer[i] = i;
  }
  volatile int sum = 0;
  for (int i = 0; i < size; ++i) {
    sum += buffer[i];
  }
  delete[] buffer;
  (void)sum;
}

int main() {
  std::printf("===========================================\n");
  std::printf("PerFlow PMU Sampler Demo\n");
  std::printf("===========================================\n\n");

  // -------------------------------------------------------------------------
  // Step 1: Create and configure the sampler
  // -------------------------------------------------------------------------
  std::printf("Step 1: Creating PMU sampler with custom configuration...\n");

  // Use smaller capacity for demo (in production, use larger values)
  PmuSampler<16, 256> sampler;

  // Configure the sampler
  PmuSamplerConfig config;
  config.frequency = 100;                          // 100 samples per second
  config.primary_event = PmuEventType::kCpuCycles; // Sample on CPU cycles
  config.overflow_threshold = 1000000;             // 1M cycles between samples
  config.max_stack_depth = 16;                     // Capture up to 16 frames
  config.enable_stack_unwinding = false;           // Disable for demo (simpler)
  config.compress_output = false;                  // Uncompressed output
  config.flush_interval_seconds = 0;               // Only flush on demand

  // Set output location
  std::strcpy(config.output_directory, "/tmp");
  std::strcpy(config.output_filename, "perflow_demo");

  std::printf("  - Sampling frequency: %lu Hz\n", config.frequency);
  std::printf("  - Overflow threshold: %lu cycles\n", config.overflow_threshold);
  std::printf("  - Max stack depth: %zu frames\n", config.max_stack_depth);
  std::printf("  - Output directory: %s\n", config.output_directory);
  std::printf("  - Output filename: %s\n\n", config.output_filename);

  // -------------------------------------------------------------------------
  // Step 2: Initialize the sampler
  // -------------------------------------------------------------------------
  std::printf("Step 2: Initializing sampler...\n");

  SamplerResult result = sampler.initialize(config);
  std::printf("  - Result: %s\n", resultToString(result));
  std::printf("  - Status: %s\n\n", statusToString(sampler.status()));

  if (result != SamplerResult::kSuccess) {
    std::fprintf(stderr, "Failed to initialize sampler!\n");
    return 1;
  }

  // -------------------------------------------------------------------------
  // Step 3: Start sampling
  // -------------------------------------------------------------------------
  std::printf("Step 3: Starting sampling...\n");

  result = sampler.start();
  std::printf("  - Result: %s\n", resultToString(result));
  std::printf("  - Status: %s\n\n", statusToString(sampler.status()));

  // -------------------------------------------------------------------------
  // Step 4: Run workload and simulate PMU overflow events
  // -------------------------------------------------------------------------
  std::printf("Step 4: Running workload with simulated PMU overflows...\n");

  // In a real scenario, handleOverflow() would be called by a signal handler
  // when the PMU counter overflows. Here we simulate this behavior.

  std::printf("  - Running compute-intensive workload...\n");
  for (int i = 0; i < 5; ++i) {
    computeIntensive(10000);
    // Simulate PMU overflow event
    sampler.handleOverflow();
  }

  std::printf("  - Running memory-intensive workload...\n");
  for (int i = 0; i < 3; ++i) {
    memoryIntensive(1000);
    // Simulate PMU overflow event
    sampler.handleOverflow();
  }

  std::printf("  - Mixed workload...\n");
  for (int i = 0; i < 2; ++i) {
    computeIntensive(5000);
    memoryIntensive(500);
    sampler.handleOverflow();
  }

  std::printf("\n");

  // -------------------------------------------------------------------------
  // Step 5: Stop sampling and show statistics
  // -------------------------------------------------------------------------
  std::printf("Step 5: Stopping sampler and collecting statistics...\n");

  result = sampler.stop();
  std::printf("  - Result: %s\n", resultToString(result));
  std::printf("  - Status: %s\n", statusToString(sampler.status()));
  std::printf("  - Total samples collected: %lu\n", sampler.sampleCount());
  std::printf("  - Overflow events: %lu\n", sampler.overflowCount());
  std::printf("  - Unique call stacks: %zu\n\n", sampler.uniqueStackCount());

  // -------------------------------------------------------------------------
  // Step 6: Analyze collected data
  // -------------------------------------------------------------------------
  std::printf("Step 6: Analyzing collected samples...\n");

  uint64_t total_count = 0;
  size_t stack_count = 0;

  sampler.samples().for_each(
      [&total_count, &stack_count](const CallStack<16>& stack, const uint64_t& count) {
        std::printf("  Stack %zu (depth=%zu, hash=0x%016lx): %lu samples\n",
                    stack_count, stack.depth(), stack.hash(), count);
        total_count += count;
        stack_count++;
      });

  std::printf("  - Total samples in map: %lu\n\n", total_count);

  // -------------------------------------------------------------------------
  // Step 7: Export data to file
  // -------------------------------------------------------------------------
  std::printf("Step 7: Exporting data to file...\n");

  DataResult export_result = sampler.flush();
  std::printf("  - Export result: %s\n",
              export_result == DataResult::kSuccess ? "Success" : "Failed");
  std::printf("  - Output file: %s/%s.pflw\n\n",
              config.output_directory, config.output_filename);

  // -------------------------------------------------------------------------
  // Step 8: Demonstrate data import
  // -------------------------------------------------------------------------
  std::printf("Step 8: Demonstrating data import...\n");

  // Create a new map to import data into
  StaticHashMap<CallStack<16>, uint64_t, 256> imported_data;
  
  char filepath[512];
  std::snprintf(filepath, sizeof(filepath), "%s/%s.pflw",
                config.output_directory, config.output_filename);
  
  DataImporter importer(filepath);
  DataResult import_result = importer.importData(imported_data);
  
  std::printf("  - Import result: %s\n",
              import_result == DataResult::kSuccess ? "Success" : "Failed");
  std::printf("  - Imported %zu unique call stacks\n\n", imported_data.size());

  // -------------------------------------------------------------------------
  // Step 9: Cleanup
  // -------------------------------------------------------------------------
  std::printf("Step 9: Cleanup...\n");

  sampler.cleanup();
  std::printf("  - Status: %s\n", statusToString(sampler.status()));

  std::printf("\n===========================================\n");
  std::printf("Demo completed successfully!\n");
  std::printf("===========================================\n");

  return 0;
}
