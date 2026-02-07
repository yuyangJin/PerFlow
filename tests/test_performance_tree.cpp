// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <thread>

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

TEST(PerformanceTreeTest, SampleCountModeExclusive) {
  // Test exclusive mode (only tracks self samples at leaf nodes)
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kExclusive);
  tree.set_process_count(1);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "app";
  frames.push_back(main_frame);

  ResolvedFrame leaf_frame;
  leaf_frame.function_name = "leaf";
  leaf_frame.library_name = "app";
  frames.push_back(leaf_frame);

  tree.insert_call_stack(frames, 0, 100);

  // Root should have total samples
  EXPECT_EQ(tree.root()->total_samples(), 100);
  
  // Main should NOT have total samples tracked in exclusive mode (only at leaf)
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->total_samples(), 0);
  EXPECT_EQ(main_node->self_samples(), 0);  // Not a leaf
  
  // Leaf should have both total and self samples in exclusive mode
  auto leaf_node = main_node->children()[0];
  EXPECT_EQ(leaf_node->total_samples(), 100);
  EXPECT_EQ(leaf_node->self_samples(), 100);
}

TEST(PerformanceTreeTest, SampleCountModeInclusive) {
  // Test inclusive mode (only tracks total samples, not self)
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kInclusive);
  tree.set_process_count(1);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "app";
  frames.push_back(main_frame);

  ResolvedFrame leaf_frame;
  leaf_frame.function_name = "leaf";
  leaf_frame.library_name = "app";
  frames.push_back(leaf_frame);

  tree.insert_call_stack(frames, 0, 100);

  // Root should have total samples
  EXPECT_EQ(tree.root()->total_samples(), 100);
  
  // Main should have total samples in inclusive mode
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->total_samples(), 100);
  EXPECT_EQ(main_node->self_samples(), 0);  // No self tracking in inclusive mode
  
  // Leaf should have total samples but NOT self samples in inclusive mode
  auto leaf_node = main_node->children()[0];
  EXPECT_EQ(leaf_node->total_samples(), 100);
  EXPECT_EQ(leaf_node->self_samples(), 0);  // No self tracking
}

TEST(PerformanceTreeTest, SampleCountModeBoth) {
  // Test both mode (tracks both total and self samples - original behavior)
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth);
  tree.set_process_count(1);

  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "app";
  frames.push_back(main_frame);

  ResolvedFrame leaf_frame;
  leaf_frame.function_name = "leaf";
  leaf_frame.library_name = "app";
  frames.push_back(leaf_frame);

  tree.insert_call_stack(frames, 0, 100);

  // Root should have total samples
  EXPECT_EQ(tree.root()->total_samples(), 100);
  
  // Main should have total samples but not self (not a leaf)
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->total_samples(), 100);
  EXPECT_EQ(main_node->self_samples(), 0);
  
  // Leaf should have both total and self samples
  auto leaf_node = main_node->children()[0];
  EXPECT_EQ(leaf_node->total_samples(), 100);
  EXPECT_EQ(leaf_node->self_samples(), 100);
}

TEST(PerformanceTreeTest, SampleCountModeSetAfterConstruction) {
  // Test that sample count mode can be set after construction
  PerformanceTree tree;
  EXPECT_EQ(tree.sample_count_mode(), SampleCountMode::kExclusive);  // Default
  
  tree.set_sample_count_mode(SampleCountMode::kBoth);
  EXPECT_EQ(tree.sample_count_mode(), SampleCountMode::kBoth);
  
  tree.set_sample_count_mode(SampleCountMode::kInclusive);
  EXPECT_EQ(tree.sample_count_mode(), SampleCountMode::kInclusive);
}

// ============================================================================
// Tests for new tree-based data access APIs
// ============================================================================

TEST(TreeNodeTest, DepthCalculation) {
  // Create a simple tree structure: root -> main -> compute -> kernel
  ResolvedFrame root_frame;
  root_frame.function_name = "[root]";
  TreeNode root_node(root_frame);
  
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  auto main_node = std::make_shared<TreeNode>(main_frame);
  root_node.add_child(main_node);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  auto compute_node = std::make_shared<TreeNode>(compute_frame);
  main_node->add_child(compute_node);
  
  ResolvedFrame kernel_frame;
  kernel_frame.function_name = "kernel";
  auto kernel_node = std::make_shared<TreeNode>(kernel_frame);
  compute_node->add_child(kernel_node);
  
  EXPECT_EQ(root_node.depth(), 0);
  EXPECT_EQ(main_node->depth(), 1);
  EXPECT_EQ(compute_node->depth(), 2);
  EXPECT_EQ(kernel_node->depth(), 3);
}

TEST(TreeNodeTest, IsLeafAndIsRoot) {
  ResolvedFrame parent_frame;
  parent_frame.function_name = "parent";
  TreeNode parent_node(parent_frame);
  
  ResolvedFrame child_frame;
  child_frame.function_name = "child";
  auto child_node = std::make_shared<TreeNode>(child_frame);
  parent_node.add_child(child_node);
  
  EXPECT_TRUE(parent_node.is_root());
  EXPECT_FALSE(parent_node.is_leaf());
  EXPECT_FALSE(child_node->is_root());
  EXPECT_TRUE(child_node->is_leaf());
}

TEST(TreeNodeTest, Siblings) {
  ResolvedFrame parent_frame;
  parent_frame.function_name = "parent";
  TreeNode parent_node(parent_frame);
  
  ResolvedFrame child1_frame;
  child1_frame.function_name = "child1";
  auto child1 = std::make_shared<TreeNode>(child1_frame);
  parent_node.add_child(child1);
  
  ResolvedFrame child2_frame;
  child2_frame.function_name = "child2";
  auto child2 = std::make_shared<TreeNode>(child2_frame);
  parent_node.add_child(child2);
  
  ResolvedFrame child3_frame;
  child3_frame.function_name = "child3";
  auto child3 = std::make_shared<TreeNode>(child3_frame);
  parent_node.add_child(child3);
  
  auto siblings_of_child1 = child1->siblings();
  EXPECT_EQ(siblings_of_child1.size(), 2);
  
  auto siblings_of_child2 = child2->siblings();
  EXPECT_EQ(siblings_of_child2.size(), 2);
  
  // Parent has no siblings (is root)
  auto siblings_of_parent = parent_node.siblings();
  EXPECT_EQ(siblings_of_parent.size(), 0);
}

TEST(TreeNodeTest, GetPath) {
  ResolvedFrame root_frame;
  root_frame.function_name = "[root]";
  TreeNode root_node(root_frame);
  
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  auto main_node = std::make_shared<TreeNode>(main_frame);
  root_node.add_child(main_node);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  auto compute_node = std::make_shared<TreeNode>(compute_frame);
  main_node->add_child(compute_node);
  
  auto path = compute_node->get_path();
  ASSERT_EQ(path.size(), 3);
  EXPECT_EQ(path[0], "[root]");
  EXPECT_EQ(path[1], "main");
  EXPECT_EQ(path[2], "compute");
}

TEST(TreeNodeTest, FindChildByName) {
  ResolvedFrame parent_frame;
  parent_frame.function_name = "parent";
  TreeNode parent_node(parent_frame);
  
  ResolvedFrame child1_frame;
  child1_frame.function_name = "child1";
  auto child1 = std::make_shared<TreeNode>(child1_frame);
  parent_node.add_child(child1);
  
  ResolvedFrame child2_frame;
  child2_frame.function_name = "child2";
  auto child2 = std::make_shared<TreeNode>(child2_frame);
  parent_node.add_child(child2);
  
  auto found = parent_node.find_child_by_name("child2");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->frame().function_name, "child2");
  
  auto not_found = parent_node.find_child_by_name("nonexistent");
  EXPECT_EQ(not_found, nullptr);
}

TEST(PerformanceTreeTest, NodeCount) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  // Insert a call stack: main -> compute -> kernel
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames.push_back(main_frame);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  frames.push_back(compute_frame);
  
  ResolvedFrame kernel_frame;
  kernel_frame.function_name = "kernel";
  frames.push_back(kernel_frame);
  
  tree.insert_call_stack(frames, 0, 10);
  
  // Should have 4 nodes: root + main + compute + kernel
  EXPECT_EQ(tree.node_count(), 4);
}

TEST(PerformanceTreeTest, MaxDepth) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  // Insert a call stack of depth 3
  std::vector<ResolvedFrame> frames;
  for (int i = 0; i < 3; ++i) {
    ResolvedFrame frame;
    frame.function_name = "func" + std::to_string(i);
    frames.push_back(frame);
  }
  
  tree.insert_call_stack(frames, 0, 10);
  
  // Max depth should be 3 (0 for root, 1 for func0, 2 for func1, 3 for func2)
  EXPECT_EQ(tree.max_depth(), 3);
}

TEST(PerformanceTreeTest, AllNodes) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames.push_back(main_frame);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  frames.push_back(compute_frame);
  
  tree.insert_call_stack(frames, 0, 10);
  
  auto all = tree.all_nodes();
  EXPECT_EQ(all.size(), 3);  // root + main + compute
}

TEST(PerformanceTreeTest, LeafNodes) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  // Insert two different call stacks
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames1.push_back(main_frame);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  frames1.push_back(compute_frame);
  tree.insert_call_stack(frames1, 0, 10);
  
  std::vector<ResolvedFrame> frames2;
  frames2.push_back(main_frame);
  
  ResolvedFrame io_frame;
  io_frame.function_name = "io";
  frames2.push_back(io_frame);
  tree.insert_call_stack(frames2, 0, 5);
  
  auto leaves = tree.leaf_nodes();
  EXPECT_EQ(leaves.size(), 2);  // compute and io are both leaves
}

TEST(PerformanceTreeTest, FindNodesByName) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames.push_back(main_frame);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  frames.push_back(compute_frame);
  tree.insert_call_stack(frames, 0, 10);
  
  auto found = tree.find_nodes_by_name("compute");
  EXPECT_EQ(found.size(), 1);
  EXPECT_EQ(found[0]->frame().function_name, "compute");
  
  auto not_found = tree.find_nodes_by_name("nonexistent");
  EXPECT_EQ(not_found.size(), 0);
}

TEST(PerformanceTreeTest, FilterBySamples) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth);
  tree.set_process_count(1);
  
  // Insert multiple call stacks
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames1.push_back(main_frame);
  
  ResolvedFrame hot_frame;
  hot_frame.function_name = "hot_function";
  frames1.push_back(hot_frame);
  tree.insert_call_stack(frames1, 0, 100);
  
  std::vector<ResolvedFrame> frames2;
  frames2.push_back(main_frame);
  
  ResolvedFrame cold_frame;
  cold_frame.function_name = "cold_function";
  frames2.push_back(cold_frame);
  tree.insert_call_stack(frames2, 0, 5);
  
  // Filter for nodes with at least 50 samples
  auto hot_nodes = tree.filter_by_samples(50);
  
  // Should include main (105 total) and hot_function (100 total)
  bool found_hot = false;
  for (const auto& node : hot_nodes) {
    if (node->frame().function_name == "hot_function") {
      found_hot = true;
    }
  }
  EXPECT_TRUE(found_hot);
}

TEST(PerformanceTreeTest, NodesAtDepth) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  for (int i = 0; i < 3; ++i) {
    ResolvedFrame frame;
    frame.function_name = "func" + std::to_string(i);
    frames.push_back(frame);
  }
  tree.insert_call_stack(frames, 0, 10);
  
  auto depth0 = tree.nodes_at_depth(0);
  EXPECT_EQ(depth0.size(), 1);  // Only root
  
  auto depth1 = tree.nodes_at_depth(1);
  EXPECT_EQ(depth1.size(), 1);  // func0
  
  auto depth2 = tree.nodes_at_depth(2);
  EXPECT_EQ(depth2.size(), 1);  // func1
  
  auto depth3 = tree.nodes_at_depth(3);
  EXPECT_EQ(depth3.size(), 1);  // func2
}

TEST(PerformanceTreeTest, TraversePreorder) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames.push_back(main_frame);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  frames.push_back(compute_frame);
  tree.insert_call_stack(frames, 0, 10);
  
  std::vector<std::string> visited;
  tree.traverse_preorder([&visited](const std::shared_ptr<TreeNode>& node) {
    visited.push_back(node->frame().function_name);
    return true;
  });
  
  // Preorder: root, main, compute
  ASSERT_EQ(visited.size(), 3);
  EXPECT_EQ(visited[0], "[root]");
  EXPECT_EQ(visited[1], "main");
  EXPECT_EQ(visited[2], "compute");
}

TEST(PerformanceTreeTest, TraverseLevelorder) {
  PerformanceTree tree;
  tree.set_process_count(1);
  
  // Create a tree with branching: root -> main -> (compute, io)
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  frames1.push_back(main_frame);
  
  ResolvedFrame compute_frame;
  compute_frame.function_name = "compute";
  frames1.push_back(compute_frame);
  tree.insert_call_stack(frames1, 0, 10);
  
  std::vector<ResolvedFrame> frames2;
  frames2.push_back(main_frame);
  
  ResolvedFrame io_frame;
  io_frame.function_name = "io";
  frames2.push_back(io_frame);
  tree.insert_call_stack(frames2, 0, 5);
  
  std::vector<std::string> visited;
  tree.traverse_levelorder([&visited](const std::shared_ptr<TreeNode>& node) {
    visited.push_back(node->frame().function_name);
    return true;
  });
  
  // Level order: root, main, then compute and io
  ASSERT_EQ(visited.size(), 4);
  EXPECT_EQ(visited[0], "[root]");
  EXPECT_EQ(visited[1], "main");
  // compute and io are at same level, order depends on insertion
}

// ============================================================================
// Tests for Concurrency Models
// ============================================================================

TEST(ConcurrencyModelTest, DefaultConcurrencyModel) {
  PerformanceTree tree;
  EXPECT_EQ(tree.concurrency_model(), ConcurrencyModel::kSerial);
}

TEST(ConcurrencyModelTest, SetConcurrencyModel) {
  PerformanceTree tree;
  
  tree.set_concurrency_model(ConcurrencyModel::kFineGrainedLock);
  EXPECT_EQ(tree.concurrency_model(), ConcurrencyModel::kFineGrainedLock);
  
  tree.set_concurrency_model(ConcurrencyModel::kThreadLocalMerge);
  EXPECT_EQ(tree.concurrency_model(), ConcurrencyModel::kThreadLocalMerge);
  
  tree.set_concurrency_model(ConcurrencyModel::kLockFree);
  EXPECT_EQ(tree.concurrency_model(), ConcurrencyModel::kLockFree);
  
  tree.set_concurrency_model(ConcurrencyModel::kSerial);
  EXPECT_EQ(tree.concurrency_model(), ConcurrencyModel::kSerial);
}

TEST(ConcurrencyModelTest, SerialModeBasic) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth, 
                       ConcurrencyModel::kSerial);
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "test";
  frames.push_back(main_frame);
  
  ResolvedFrame work_frame;
  work_frame.function_name = "work";
  work_frame.library_name = "test";
  frames.push_back(work_frame);
  
  tree.insert_call_stack(frames, 0, 100, 1000.0);
  
  EXPECT_EQ(tree.total_samples(), 100);
  EXPECT_EQ(tree.root()->children().size(), 1);
}

TEST(ConcurrencyModelTest, FineGrainedLockBasic) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                       ConcurrencyModel::kFineGrainedLock);
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "test";
  frames.push_back(main_frame);
  
  ResolvedFrame work_frame;
  work_frame.function_name = "work";
  work_frame.library_name = "test";
  frames.push_back(work_frame);
  
  tree.insert_call_stack(frames, 0, 100, 1000.0);
  
  EXPECT_EQ(tree.total_samples(), 100);
  EXPECT_EQ(tree.root()->children().size(), 1);
}

TEST(ConcurrencyModelTest, LockFreeBasic) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                       ConcurrencyModel::kLockFree);
  tree.set_process_count(1);
  
  std::vector<ResolvedFrame> frames;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "test";
  frames.push_back(main_frame);
  
  ResolvedFrame work_frame;
  work_frame.function_name = "work";
  work_frame.library_name = "test";
  frames.push_back(work_frame);
  
  tree.insert_call_stack(frames, 0, 100, 1000.0);
  tree.consolidate_atomic_counters();
  
  EXPECT_EQ(tree.total_samples(), 100);
  EXPECT_EQ(tree.root()->children().size(), 1);
}

TEST(ConcurrencyModelTest, ThreadLocalMergeBasic) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                       ConcurrencyModel::kThreadLocalMerge);
  tree.set_process_count(2);
  
  // Create two thread-local trees
  auto local_tree1 = tree.create_thread_local_tree();
  auto local_tree2 = tree.create_thread_local_tree();
  
  // Insert into first tree
  std::vector<ResolvedFrame> frames1;
  ResolvedFrame main_frame;
  main_frame.function_name = "main";
  main_frame.library_name = "test";
  frames1.push_back(main_frame);
  
  ResolvedFrame work1_frame;
  work1_frame.function_name = "work1";
  work1_frame.library_name = "test";
  frames1.push_back(work1_frame);
  
  local_tree1->insert_call_stack(frames1, 0, 50, 500.0);
  
  // Insert into second tree
  std::vector<ResolvedFrame> frames2;
  frames2.push_back(main_frame);
  
  ResolvedFrame work2_frame;
  work2_frame.function_name = "work2";
  work2_frame.library_name = "test";
  frames2.push_back(work2_frame);
  
  local_tree2->insert_call_stack(frames2, 1, 30, 300.0);
  
  // Merge trees
  tree.merge_thread_local_tree(*local_tree1);
  tree.merge_thread_local_tree(*local_tree2);
  
  EXPECT_EQ(tree.total_samples(), 80);  // 50 + 30
  EXPECT_EQ(tree.root()->children().size(), 1);  // Only main
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->children().size(), 2);  // work1 and work2
}

// Test concurrent insertion with fine-grained locking
TEST(ConcurrencyModelTest, FineGrainedLockConcurrent) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                       ConcurrencyModel::kFineGrainedLock);
  tree.set_process_count(4);
  
  // Create call stack template
  auto create_frames = [](const std::string& leaf_name) {
    std::vector<ResolvedFrame> frames;
    ResolvedFrame main_frame;
    main_frame.function_name = "main";
    main_frame.library_name = "test";
    frames.push_back(main_frame);
    
    ResolvedFrame leaf_frame;
    leaf_frame.function_name = leaf_name;
    leaf_frame.library_name = "test";
    frames.push_back(leaf_frame);
    return frames;
  };
  
  // Insert from multiple threads
  std::vector<std::thread> threads;
  for (size_t i = 0; i < 4; ++i) {
    threads.emplace_back([&tree, &create_frames, i]() {
      auto frames = create_frames("worker" + std::to_string(i));
      for (int j = 0; j < 100; ++j) {
        tree.insert_call_stack(frames, i, 1, 10.0);
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  // Verify results
  EXPECT_EQ(tree.total_samples(), 400);  // 4 threads * 100 insertions
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->children().size(), 4);  // 4 different workers
}

// Test concurrent insertion with lock-free model
TEST(ConcurrencyModelTest, LockFreeConcurrent) {
  PerformanceTree tree(TreeBuildMode::kContextFree, SampleCountMode::kBoth,
                       ConcurrencyModel::kLockFree);
  tree.set_process_count(4);
  
  // Create call stack template - all threads insert to same leaf
  auto create_frames = []() {
    std::vector<ResolvedFrame> frames;
    ResolvedFrame main_frame;
    main_frame.function_name = "main";
    main_frame.library_name = "test";
    frames.push_back(main_frame);
    
    ResolvedFrame leaf_frame;
    leaf_frame.function_name = "shared_worker";
    leaf_frame.library_name = "test";
    frames.push_back(leaf_frame);
    return frames;
  };
  
  // Insert from multiple threads to same node
  std::vector<std::thread> threads;
  for (size_t i = 0; i < 4; ++i) {
    threads.emplace_back([&tree, &create_frames, i]() {
      auto frames = create_frames();
      for (int j = 0; j < 100; ++j) {
        tree.insert_call_stack(frames, i, 1, 10.0);
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  // Consolidate atomic counters
  tree.consolidate_atomic_counters();
  
  // Verify results
  EXPECT_EQ(tree.total_samples(), 400);  // 4 threads * 100 insertions
  auto main_node = tree.root()->children()[0];
  EXPECT_EQ(main_node->children().size(), 1);  // Only shared_worker
  auto worker_node = main_node->children()[0];
  EXPECT_EQ(worker_node->total_samples(), 400);  // All samples converged
}

TEST(ConcurrencyModelTest, NumThreadsConfiguration) {
  PerformanceTree tree;
  
  EXPECT_GT(tree.num_threads(), 0);  // Should have at least 1 thread
  
  tree.set_num_threads(8);
  EXPECT_EQ(tree.num_threads(), 8);
  
  tree.set_num_threads(0);  // Should fall back to 1
  EXPECT_EQ(tree.num_threads(), 1);
}
