// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "analysis/tree_builder.h"
#include "sampling/data_export.h"
#include "sampling/static_hash_map.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

TEST(TreeBuilderTest, BasicConstruction) {
  TreeBuilder builder;
  
  EXPECT_NE(builder.tree().root(), nullptr);
  EXPECT_EQ(builder.tree().total_samples(), 0);
}

TEST(TreeBuilderTest, BuildFromFile) {
  // Create a temporary sample file
  const char* test_dir = "/tmp";
  const char* test_file = "test_tree_builder";

  // Create sample data
  StaticHashMap<CallStack<>, uint64_t, 1024> data;
  
  uintptr_t addresses1[] = {0x1000, 0x2000, 0x3000};
  CallStack<> stack1(addresses1, 3);
  data.insert(stack1, 10);

  uintptr_t addresses2[] = {0x1000, 0x2000, 0x4000};
  CallStack<> stack2(addresses2, 3);
  data.insert(stack2, 5);

  // Export data
  DataExporter exporter(test_dir, test_file, false);
  auto result = exporter.exportData(data);
  ASSERT_EQ(result, DataResult::kSuccess);

  // Build tree from file
  TreeBuilder builder;
  char filepath[256];
  std::snprintf(filepath, sizeof(filepath), "%s/%s.pflw", test_dir, test_file);
  
  bool success = builder.build_from_file(filepath, 0, 1000.0);
  EXPECT_TRUE(success);
  EXPECT_EQ(builder.tree().total_samples(), 15);

  // Clean up
  std::remove(filepath);
}

TEST(TreeBuilderTest, BuildFromMultipleFiles) {
  const char* test_dir = "/tmp";

  std::vector<std::pair<std::string, uint32_t>> files;

  // Create multiple sample files
  for (int i = 0; i < 3; ++i) {
    std::string test_file = "test_tree_builder_" + std::to_string(i);
    
    StaticHashMap<CallStack<>, uint64_t, 1024> data;
    
    uintptr_t addresses[] = {0x1000, 0x2000, 0x3000};
    CallStack<> stack(addresses, 3);
    data.insert(stack, 10 * (i + 1));

    DataExporter exporter(test_dir, test_file.c_str(), false);
    auto result = exporter.exportData(data);
    ASSERT_EQ(result, DataResult::kSuccess);

    std::string filepath = std::string(test_dir) + "/" + test_file + ".pflw";
    files.push_back({filepath, static_cast<uint32_t>(i)});
  }

  // Build tree from files
  TreeBuilder builder;
  size_t success_count = builder.build_from_files(files, 1000.0);
  
  EXPECT_EQ(success_count, 3);
  EXPECT_EQ(builder.tree().total_samples(), 10 + 20 + 30);
  EXPECT_EQ(builder.tree().process_count(), 3);

  // Clean up
  for (const auto& pair : files) {
    std::remove(pair.first.c_str());
  }
}

TEST(TreeBuilderTest, Clear) {
  const char* test_dir = "/tmp";
  const char* test_file = "test_tree_builder_clear";

  // Create sample data
  StaticHashMap<CallStack<>, uint64_t, 1024> data;
  
  uintptr_t addresses[] = {0x1000, 0x2000};
  CallStack<> stack(addresses, 2);
  data.insert(stack, 100);

  DataExporter exporter(test_dir, test_file, false);
  auto result = exporter.exportData(data);
  ASSERT_EQ(result, DataResult::kSuccess);

  // Build tree
  TreeBuilder builder;
  char filepath[256];
  std::snprintf(filepath, sizeof(filepath), "%s/%s.pflw", test_dir, test_file);
  
  builder.build_from_file(filepath, 0, 1000.0);
  EXPECT_GT(builder.tree().total_samples(), 0);

  // Clear
  builder.clear();
  EXPECT_EQ(builder.tree().total_samples(), 0);
  EXPECT_EQ(builder.converter().snapshot_count(), 0);

  // Clean up
  std::remove(filepath);
}
