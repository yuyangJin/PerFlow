// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "analysis/performance_tree.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

TEST(PerformanceTreeTest, BasicConstruction) {
  PerformanceTree tree;
  EXPECT_NE(tree.root(), nullptr);
  EXPECT_EQ(tree.process_count(), 0);
  EXPECT_EQ(tree.total_samples(), 0);
}

TEST(PerformanceTreeTest, SetProcessCount) {
  PerformanceTree tree;
  tree.set_process_count(4);
  EXPECT_EQ(tree.process_count(), 4);
}

TEST(PerformanceTreeTest, InsertCallStack) {
  PerformanceTree tree;
  tree.set_process_count(2);

  // Create a simple call stack
  std::vector<ResolvedFrame> frames;
  
  ResolvedFrame frame1;
  frame1.function_name = "main";
  frame1.library_name = "test_app";
  frames.push_back(frame1);

  ResolvedFrame frame2;
  frame2.function_name = "compute";
  frame2.library_name = "test_app";
  frames.push_back(frame2);

  ResolvedFrame frame3;
  frame3.function_name = "kernel";
  frame3.library_name = "test_app";
  frames.push_back(frame3);

  // Insert the call stack
  tree.insert_call_stack(frames, 0, 10, 1000.0);

  EXPECT_EQ(tree.total_samples(), 10);
  EXPECT_NE(tree.root(), nullptr);
  EXPECT_FALSE(tree.root()->children().empty());
}

TEST(PerformanceTreeTest, InsertMultipleCallStacks) {
  PerformanceTree tree;
  tree.set_process_count(2);

  // Insert first call stack
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame frame1;
  frame1.function_name = "main";
  frame1.library_name = "test_app";
  frames1.push_back(frame1);

  ResolvedFrame frame2;
  frame2.function_name = "compute";
  frame2.library_name = "test_app";
  frames1.push_back(frame2);

  tree.insert_call_stack(frames1, 0, 5, 500.0);

  // Insert second call stack with different leaf
  std::vector<ResolvedFrame> frames2;
  frames2.push_back(frame1);
  ResolvedFrame frame3;
  frame3.function_name = "io";
  frame3.library_name = "test_app";
  frames2.push_back(frame3);

  tree.insert_call_stack(frames2, 0, 3, 300.0);

  EXPECT_EQ(tree.total_samples(), 8);
  
  // Root should have one child (main)
  EXPECT_EQ(tree.root()->children().size(), 1);
  
  // Main should have two children (compute and io)
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->children().size(), 2);
}

TEST(PerformanceTreeTest, MultiProcessSamples) {
  PerformanceTree tree;
  tree.set_process_count(4);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame frame;
  frame.function_name = "work";
  frame.library_name = "test_app";
  frames.push_back(frame);

  // Insert samples from different processes
  for (size_t i = 0; i < 4; ++i) {
    tree.insert_call_stack(frames, i, i + 1, (i + 1) * 100.0);
  }

  EXPECT_EQ(tree.total_samples(), 1 + 2 + 3 + 4);
  
  auto work_node = tree.root()->children()[0];
  const auto& counts = work_node->sampling_counts();
  EXPECT_EQ(counts.size(), 4);
  EXPECT_EQ(counts[0], 1);
  EXPECT_EQ(counts[1], 2);
  EXPECT_EQ(counts[2], 3);
  EXPECT_EQ(counts[3], 4);
}

TEST(PerformanceTreeTest, Clear) {
  PerformanceTree tree;
  tree.set_process_count(2);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame frame;
  frame.function_name = "test";
  frame.library_name = "test_app";
  frames.push_back(frame);

  tree.insert_call_stack(frames, 0, 5, 500.0);
  EXPECT_EQ(tree.total_samples(), 5);

  tree.clear();
  EXPECT_EQ(tree.total_samples(), 0);
  EXPECT_EQ(tree.root()->children().size(), 0);
}

TEST(TreeNodeTest, AddSample) {
  ResolvedFrame frame;
  frame.function_name = "test";
  TreeNode node(frame);
  
  node.set_process_count(2);
  node.add_sample(0, 10, 1000.0);
  node.add_sample(1, 5, 500.0);

  EXPECT_EQ(node.total_samples(), 15);
  EXPECT_EQ(node.sampling_counts()[0], 10);
  EXPECT_EQ(node.sampling_counts()[1], 5);
  EXPECT_DOUBLE_EQ(node.execution_times()[0], 1000.0);
  EXPECT_DOUBLE_EQ(node.execution_times()[1], 500.0);
}

TEST(TreeNodeTest, FindChild) {
  ResolvedFrame parent_frame;
  parent_frame.function_name = "parent";
  TreeNode parent(parent_frame);

  ResolvedFrame child_frame1;
  child_frame1.function_name = "child1";
  child_frame1.library_name = "lib1";
  auto child1 = std::make_shared<TreeNode>(child_frame1);
  parent.add_child(child1);

  ResolvedFrame child_frame2;
  child_frame2.function_name = "child2";
  child_frame2.library_name = "lib2";
  auto child2 = std::make_shared<TreeNode>(child_frame2);
  parent.add_child(child2);

  // Find existing child
  auto found = parent.find_child(child_frame1);
  EXPECT_NE(found, nullptr);
  EXPECT_EQ(found->frame().function_name, "child1");

  // Find non-existing child
  ResolvedFrame non_existing;
  non_existing.function_name = "non_existing";
  auto not_found = parent.find_child(non_existing);
  EXPECT_EQ(not_found, nullptr);
}

TEST(TreeNodeTest, CallCounts) {
  ResolvedFrame parent_frame;
  parent_frame.function_name = "parent";
  TreeNode parent(parent_frame);

  ResolvedFrame child_frame;
  child_frame.function_name = "child";
  auto child = std::make_shared<TreeNode>(child_frame);
  parent.add_child(child);

  parent.increment_call_count(child, 5);
  EXPECT_EQ(parent.get_call_count(child), 5);

  parent.increment_call_count(child, 3);
  EXPECT_EQ(parent.get_call_count(child), 8);
}

// Test context-aware vs context-free tree building modes
TEST(PerformanceTreeTest, ContextAwareMode) {
  // Create test frames
  ResolvedFrame main_frame, func_a, func_b;
  main_frame.function_name = "main";
  main_frame.library_name = "test";
  main_frame.offset = 0x1000;
  
  func_a.function_name = "func";
  func_a.library_name = "test";
  func_a.offset = 0x2000;
  
  func_b.function_name = "func";  // Same function name and library
  func_b.library_name = "test";
  func_b.offset = 0x3000;          // Different offset (different call site)
  
  // Test context-free mode (default) - should merge nodes with same function name
  PerformanceTree tree_free(TreeBuildMode::kContextFree);
  tree_free.set_process_count(1);
  
  std::vector<ResolvedFrame> stack1 = {main_frame, func_a};
  tree_free.insert_call_stack(stack1, 0, 10);
  
  std::vector<ResolvedFrame> stack2 = {main_frame, func_b};
  tree_free.insert_call_stack(stack2, 0, 5);
  
  EXPECT_EQ(tree_free.root()->children().size(), 1);  // Only main
  auto main_node = tree_free.root()->children()[0];
  EXPECT_EQ(main_node->children().size(), 1);  // func merged
  EXPECT_EQ(main_node->children()[0]->total_samples(), 15);  // 10 + 5
  
  // Test context-aware mode - should keep separate nodes for different offsets
  PerformanceTree tree_aware(TreeBuildMode::kContextAware);
  tree_aware.set_process_count(1);
  
  tree_aware.insert_call_stack(stack1, 0, 10);
  tree_aware.insert_call_stack(stack2, 0, 5);
  
  EXPECT_EQ(tree_aware.root()->children().size(), 1);  // Only main
  auto main_node_aware = tree_aware.root()->children()[0];
  EXPECT_EQ(main_node_aware->children().size(), 2);  // func@0x2000 and func@0x3000
  EXPECT_EQ(main_node_aware->children()[0]->total_samples(), 10);
  EXPECT_EQ(main_node_aware->children()[1]->total_samples(), 5);
}
