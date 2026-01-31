// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "analysis/analyzers.h"
#include "analysis/performance_tree.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

TEST(BalanceAnalyzerTest, BasicAnalysis) {
  PerformanceTree tree;
  tree.set_process_count(4);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame frame;
  frame.function_name = "work";
  frame.library_name = "test_app";
  frames.push_back(frame);

  // Create balanced workload
  for (size_t i = 0; i < 4; ++i) {
    tree.insert_call_stack(frames, i, 100, 10000.0);
  }

  auto result = BalanceAnalyzer::analyze(tree);

  EXPECT_DOUBLE_EQ(result.mean_samples, 100.0);
  EXPECT_DOUBLE_EQ(result.min_samples, 100.0);
  EXPECT_DOUBLE_EQ(result.max_samples, 100.0);
  EXPECT_DOUBLE_EQ(result.std_dev_samples, 0.0);
  EXPECT_DOUBLE_EQ(result.imbalance_factor, 0.0);
}

TEST(BalanceAnalyzerTest, ImbalancedWorkload) {
  PerformanceTree tree;
  tree.set_process_count(4);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame frame;
  frame.function_name = "work";
  frame.library_name = "test_app";
  frames.push_back(frame);

  // Create imbalanced workload
  tree.insert_call_stack(frames, 0, 50, 5000.0);
  tree.insert_call_stack(frames, 1, 100, 10000.0);
  tree.insert_call_stack(frames, 2, 150, 15000.0);
  tree.insert_call_stack(frames, 3, 200, 20000.0);

  auto result = BalanceAnalyzer::analyze(tree);

  EXPECT_DOUBLE_EQ(result.mean_samples, 125.0);
  EXPECT_DOUBLE_EQ(result.min_samples, 50.0);
  EXPECT_DOUBLE_EQ(result.max_samples, 200.0);
  EXPECT_EQ(result.least_loaded_process, 0);
  EXPECT_EQ(result.most_loaded_process, 3);
  EXPECT_GT(result.imbalance_factor, 0.0);
  EXPECT_GT(result.std_dev_samples, 0.0);
}

TEST(HotspotAnalyzerTest, FindHotspots) {
  PerformanceTree tree;
  tree.set_process_count(1);

  // Create multiple functions with different sample counts
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "app";
  frames1.push_back(main_frame);

  ResolvedFrame hot_frame;
  hot_frame.function_name = "hot_function";
  hot_frame.library_name = "app";
  frames1.push_back(hot_frame);

  std::vector<ResolvedFrame> frames2;
  frames2.push_back(main_frame);
  ResolvedFrame cold_frame;
  cold_frame.function_name = "cold_function";
  cold_frame.library_name = "app";
  frames2.push_back(cold_frame);

  // Insert more samples for hot function
  tree.insert_call_stack(frames1, 0, 900, 90000.0);
  tree.insert_call_stack(frames2, 0, 100, 10000.0);

  auto hotspots = HotspotAnalyzer::find_hotspots(tree, 5);

  EXPECT_GE(hotspots.size(), 1);
  
  // Main should be first (has all samples)
  EXPECT_EQ(hotspots[0].function_name, "main");
  EXPECT_EQ(hotspots[0].total_samples, 1000);

  // Hot function should be second
  EXPECT_EQ(hotspots[1].function_name, "hot_function");
  EXPECT_EQ(hotspots[1].total_samples, 900);
  EXPECT_NEAR(hotspots[1].percentage, 90.0, 0.1);
}

TEST(HotspotAnalyzerTest, FindSelfHotspots) {
  PerformanceTree tree;
  tree.set_process_count(1);

  // Create call stacks where leaf nodes are the actual hotspots
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "app";
  frames1.push_back(main_frame);

  ResolvedFrame leaf1;
  leaf1.function_name = "leaf_hot";
  leaf1.library_name = "app";
  frames1.push_back(leaf1);

  std::vector<ResolvedFrame> frames2;
  frames2.push_back(main_frame);
  ResolvedFrame leaf2;
  leaf2.function_name = "leaf_cold";
  leaf2.library_name = "app";
  frames2.push_back(leaf2);

  tree.insert_call_stack(frames1, 0, 800, 80000.0);
  tree.insert_call_stack(frames2, 0, 200, 20000.0);

  auto self_hotspots = HotspotAnalyzer::find_self_hotspots(tree, 5);

  EXPECT_GE(self_hotspots.size(), 1);
  
  // Leaf_hot should be first with highest self samples
  EXPECT_EQ(self_hotspots[0].function_name, "leaf_hot");
  EXPECT_EQ(self_hotspots[0].self_samples, 800);
}

TEST(HotspotAnalyzerTest, EmptyTree) {
  PerformanceTree tree;
  tree.set_process_count(1);

  auto hotspots = HotspotAnalyzer::find_hotspots(tree, 10);
  EXPECT_EQ(hotspots.size(), 0);

  auto self_hotspots = HotspotAnalyzer::find_self_hotspots(tree, 10);
  EXPECT_EQ(self_hotspots.size(), 0);
}

TEST(HotspotAnalyzerTest, TopNLimit) {
  PerformanceTree tree;
  tree.set_process_count(1);

  // Create many functions
  for (int i = 0; i < 20; ++i) {
    std::vector<ResolvedFrame> frames;
    ResolvedFrame frame;
    frame.function_name = "func_" + std::to_string(i);
    frame.library_name = "app";
    frames.push_back(frame);

    tree.insert_call_stack(frames, 0, 100 - i * 4, (100 - i * 4) * 100.0);
  }

  auto hotspots = HotspotAnalyzer::find_hotspots(tree, 5);
  EXPECT_EQ(hotspots.size(), 5);

  // Verify they're sorted by sample count
  for (size_t i = 1; i < hotspots.size(); ++i) {
    EXPECT_GE(hotspots[i - 1].total_samples, hotspots[i].total_samples);
  }
}
