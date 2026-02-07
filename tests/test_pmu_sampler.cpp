// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "sampling/pmu_sampler.h"

using namespace perflow::sampling;

// Use smaller capacity for tests to avoid stack overflow
constexpr size_t kTestStackDepth = 16;
constexpr size_t kTestMapCapacity = 256;
using TestPmuSampler = PmuSampler<kTestStackDepth, kTestMapCapacity>;

class PmuSamplerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::system("mkdir -p /tmp/perflow_test");
  }

  void TearDown() override {
    std::system("rm -f /tmp/perflow_test/*.pflw");
  }
};

TEST_F(PmuSamplerTest, DefaultConstruction) {
  TestPmuSampler sampler;

  EXPECT_EQ(sampler.status(), SamplerStatus::kUninitialized);
  EXPECT_EQ(sampler.sampleCount(), 0u);
  EXPECT_EQ(sampler.overflowCount(), 0u);
  EXPECT_EQ(sampler.uniqueStackCount(), 0u);
}

TEST_F(PmuSamplerTest, ConfigDefaults) {
  PmuSamplerConfig config;

  EXPECT_EQ(config.frequency, kDefaultSamplingFrequency);
  EXPECT_EQ(config.primary_event, PmuEventType::kCpuCycles);
  EXPECT_EQ(config.overflow_threshold, kDefaultOverflowThreshold);
  EXPECT_EQ(config.max_stack_depth, kDefaultMaxStackDepth);
  EXPECT_TRUE(config.enable_stack_unwinding);
  EXPECT_FALSE(config.compress_output);
  EXPECT_EQ(config.flush_interval_seconds, 0u);
}

TEST_F(PmuSamplerTest, Initialize) {
  TestPmuSampler sampler;
  PmuSamplerConfig config;

  std::strcpy(config.output_directory, "/tmp/perflow_test");
  std::strcpy(config.output_filename, "test_init");

  SamplerResult result = sampler.initialize(config);

  EXPECT_EQ(result, SamplerResult::kSuccess);
  EXPECT_EQ(sampler.status(), SamplerStatus::kInitialized);
}

TEST_F(PmuSamplerTest, StartAndStop) {
  TestPmuSampler sampler;
  PmuSamplerConfig config;

  std::strcpy(config.output_directory, "/tmp/perflow_test");

  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);

  EXPECT_EQ(sampler.start(), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.status(), SamplerStatus::kRunning);

  EXPECT_EQ(sampler.stop(), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.status(), SamplerStatus::kStopped);
}

TEST_F(PmuSamplerTest, HandleOverflow) {
  PmuSampler<8, 64> sampler;
  PmuSamplerConfig config;
  config.enable_stack_unwinding = false;  // Simpler for testing

  std::strcpy(config.output_directory, "/tmp/perflow_test");

  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.start(), SamplerResult::kSuccess);

  // Simulate overflow events
  sampler.handleOverflow();
  sampler.handleOverflow();
  sampler.handleOverflow();

  EXPECT_EQ(sampler.overflowCount(), 3u);
  EXPECT_EQ(sampler.sampleCount(), 3u);
  EXPECT_GE(sampler.uniqueStackCount(), 1u);

  sampler.stop();
}

TEST_F(PmuSamplerTest, FlushData) {
  PmuSampler<8, 64> sampler;
  PmuSamplerConfig config;
  config.enable_stack_unwinding = false;

  std::strcpy(config.output_directory, "/tmp/perflow_test");
  std::strcpy(config.output_filename, "test_flush");

  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.start(), SamplerResult::kSuccess);

  // Generate some samples
  for (int i = 0; i < 10; ++i) {
    sampler.handleOverflow();
  }

  sampler.stop();

  // Flush to disk
  DataResult result = sampler.flush();
  EXPECT_EQ(result, DataResult::kSuccess);

  // Verify file exists
  FILE* f = std::fopen("/tmp/perflow_test/test_flush.pflw", "rb");
  ASSERT_NE(f, nullptr);
  std::fclose(f);
}

TEST_F(PmuSamplerTest, UpdateConfig) {
  TestPmuSampler sampler;
  PmuSamplerConfig config;

  std::strcpy(config.output_directory, "/tmp/perflow_test");

  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);

  // Update config before running
  PmuSamplerConfig new_config = config;
  new_config.frequency = 200;
  EXPECT_EQ(sampler.updateConfig(new_config), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.config().frequency, 200u);
}

TEST_F(PmuSamplerTest, Cleanup) {
  TestPmuSampler sampler;
  PmuSamplerConfig config;

  std::strcpy(config.output_directory, "/tmp/perflow_test");

  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.start(), SamplerResult::kSuccess);

  sampler.cleanup();

  EXPECT_EQ(sampler.status(), SamplerStatus::kUninitialized);
}

TEST_F(PmuSamplerTest, CustomStackDepth) {
  PmuSampler<16, 32> small_sampler;
  PmuSampler<64, 128> medium_sampler;

  PmuSamplerConfig config;
  std::strcpy(config.output_directory, "/tmp/perflow_test");

  EXPECT_EQ(small_sampler.initialize(config), SamplerResult::kSuccess);
  EXPECT_EQ(medium_sampler.initialize(config), SamplerResult::kSuccess);
}

TEST_F(PmuSamplerTest, ReInitialize) {
  TestPmuSampler sampler;
  PmuSamplerConfig config;

  std::strcpy(config.output_directory, "/tmp/perflow_test");

  // First initialization
  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.start(), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.stop(), SamplerResult::kSuccess);

  // Re-initialize should work after stop
  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);
  EXPECT_EQ(sampler.status(), SamplerStatus::kInitialized);
}

TEST_F(PmuSamplerTest, StartWithoutInitialize) {
  TestPmuSampler sampler;

  // Should fail if not initialized
  SamplerResult result = sampler.start();
  EXPECT_EQ(result, SamplerResult::kErrorInitialization);
}

TEST_F(PmuSamplerTest, SamplerWithSmallCapacity) {
  // Verify a sampler with small capacity works properly
  PmuSampler<8, 64> sampler;
  PmuSamplerConfig config;

  std::strcpy(config.output_directory, "/tmp/perflow_test");

  EXPECT_EQ(sampler.initialize(config), SamplerResult::kSuccess);
}
