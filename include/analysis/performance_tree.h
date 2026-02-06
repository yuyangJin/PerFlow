// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
#define PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "sampling/call_stack.h"

namespace perflow {
namespace analysis {

/// TreeBuildMode defines how call stacks are aggregated into a tree
enum class TreeBuildMode {
  /// Calling-context-free: Nodes with the same function are merged
  /// regardless of their calling context (call path)
  kContextFree = 0,
  
  /// Calling-context-aware: Nodes are distinguished by their full
  /// calling context (call path), creating separate nodes for the
  /// same function called from different contexts
  kContextAware = 1
};

/// SampleCountMode defines how samples are counted during tree building
enum class SampleCountMode {
  /// Exclusive: Only track self samples (samples at leaf nodes)
  /// Nodes will only have self_samples set, total_samples will equal self_samples
  kExclusive = 0,
  
  /// Inclusive: Only track total samples (all samples including children)
  /// Nodes will have total_samples set from all paths through them
  kInclusive = 1,
  
  /// Both: Track both inclusive and exclusive samples (default)
  /// Nodes will have both total_samples and self_samples tracked independently
  kBoth = 2
};

/// TreeNode represents a vertex in the performance tree
/// Each node corresponds to a function/program structure
class TreeNode {
 public:
  /// Constructor with frame information
  explicit TreeNode(const sampling::ResolvedFrame& frame) noexcept
      : frame_(frame),
        sampling_counts_(),
        execution_times_(),
        children_(),
        parent_(nullptr),
        total_samples_(0),
        self_samples_(0) {}

  /// Get the resolved frame
  const sampling::ResolvedFrame& frame() const noexcept { return frame_; }

  /// Get sampling counts for all processes
  const std::vector<uint64_t>& sampling_counts() const noexcept {
    return sampling_counts_;
  }

  /// Get execution times for all processes (in microseconds)
  const std::vector<double>& execution_times() const noexcept {
    return execution_times_;
  }

  /// Set the number of processes
  void set_process_count(size_t count) {
    if (sampling_counts_.size() != count) {
      sampling_counts_.resize(count, 0);
      execution_times_.resize(count, 0.0);
    }
  }

  /// Add a sample for a specific process
  void add_sample(size_t process_id, uint64_t count = 1,
                  double time_us = 0.0) {
    if (process_id >= sampling_counts_.size()) {
      set_process_count(process_id + 1);
    }
    sampling_counts_[process_id] += count;
    execution_times_[process_id] += time_us;
    total_samples_ += count;
  }

  /// Add self samples (leaf node samples)
  void add_self_sample(uint64_t count = 1) { self_samples_ += count; }

  /// Get total samples across all processes
  uint64_t total_samples() const noexcept { return total_samples_; }

  /// Get self samples (samples at this leaf)
  uint64_t self_samples() const noexcept { return self_samples_; }

  /// Get children nodes
  const std::vector<std::shared_ptr<TreeNode>>& children() const noexcept {
    return children_;
  }

  /// Get parent node
  TreeNode* parent() const noexcept { return parent_; }

  /// Add a child node
  void add_child(std::shared_ptr<TreeNode> child) {
    child->parent_ = this;
    children_.push_back(child);
  }

  /// Find a child by frame information (function name and library)
  /// Context-free mode: Only checks function and library names
  std::shared_ptr<TreeNode> find_child(
      const sampling::ResolvedFrame& frame) const {
    for (const auto& child : children_) {
      if (child->frame_.function_name == frame.function_name &&
          child->frame_.library_name == frame.library_name) {
        return child;
      }
    }
    return nullptr;
  }
  
  /// Find a child by full frame information including calling context
  /// Context-aware mode: Checks all frame attributes including offset
  std::shared_ptr<TreeNode> find_child_context_aware(
      const sampling::ResolvedFrame& frame) const {
    for (const auto& child : children_) {
      if (child->frame_.function_name == frame.function_name &&
          child->frame_.library_name == frame.library_name &&
          child->frame_.offset == frame.offset) {
        return child;
      }
    }
    return nullptr;
  }

  /// Get call count to a specific child
  uint64_t get_call_count(const std::shared_ptr<TreeNode>& child) const {
    auto it = call_counts_.find(child.get());
    return it != call_counts_.end() ? it->second : 0;
  }

  /// Increment call count to a specific child
  void increment_call_count(const std::shared_ptr<TreeNode>& child,
                            uint64_t count = 1) {
    call_counts_[child.get()] += count;
  }

  /// Get the depth of this node in the tree (root = 0)
  size_t depth() const noexcept {
    size_t d = 0;
    TreeNode* p = parent_;
    while (p != nullptr) {
      ++d;
      p = p->parent_;
    }
    return d;
  }

  /// Get the number of children
  size_t child_count() const noexcept { return children_.size(); }

  /// Check if this is a leaf node (no children)
  bool is_leaf() const noexcept { return children_.empty(); }

  /// Check if this is the root node (no parent)
  bool is_root() const noexcept { return parent_ == nullptr; }

  /// Get all sibling nodes (nodes with the same parent)
  std::vector<std::shared_ptr<TreeNode>> siblings() const {
    std::vector<std::shared_ptr<TreeNode>> result;
    if (parent_ == nullptr) {
      return result;
    }
    for (const auto& child : parent_->children_) {
      if (child.get() != this) {
        result.push_back(child);
      }
    }
    return result;
  }

  /// Get the path from root to this node as a vector of function names
  std::vector<std::string> get_path() const {
    std::vector<std::string> path;
    const TreeNode* current = this;
    while (current != nullptr) {
      path.push_back(current->frame_.function_name);
      current = current->parent_;
    }
    std::reverse(path.begin(), path.end());
    return path;
  }

  /// Find child by function name
  std::shared_ptr<TreeNode> find_child_by_name(const std::string& func_name) const {
    for (const auto& child : children_) {
      if (child->frame_.function_name == func_name) {
        return child;
      }
    }
    return nullptr;
  }

  /// Get total execution time across all processes (in microseconds)
  double total_execution_time() const noexcept {
    double total = 0.0;
    for (double t : execution_times_) {
      total += t;
    }
    return total;
  }

  /// Get execution time for a specific process
  double execution_time(size_t process_id) const noexcept {
    if (process_id >= execution_times_.size()) {
      return 0.0;
    }
    return execution_times_[process_id];
  }

  /// Get sampling count for a specific process
  uint64_t sampling_count(size_t process_id) const noexcept {
    if (process_id >= sampling_counts_.size()) {
      return 0;
    }
    return sampling_counts_[process_id];
  }

 private:
  sampling::ResolvedFrame frame_;
  std::vector<uint64_t> sampling_counts_;   // Per-process sample counts
  std::vector<double> execution_times_;     // Per-process execution times (us)
  std::vector<std::shared_ptr<TreeNode>> children_;
  TreeNode* parent_;
  std::unordered_map<TreeNode*, uint64_t> call_counts_;  // Edge weights
  uint64_t total_samples_;
  uint64_t self_samples_;
};

/// PerformanceTree aggregates call stack samples into a tree structure
/// Thread-safe for concurrent insertions
class PerformanceTree {
 public:
  /// Constructor with optional build mode and sample count mode
  explicit PerformanceTree(TreeBuildMode mode = TreeBuildMode::kContextFree,
                          SampleCountMode count_mode = SampleCountMode::kExclusive) noexcept 
      : root_(nullptr), process_count_(0), build_mode_(mode), 
        sample_count_mode_(count_mode), mutex_() {
    // Create a virtual root node
    sampling::ResolvedFrame root_frame;
    root_frame.function_name = "[root]";
    root_frame.library_name = "[virtual]";
    root_ = std::make_shared<TreeNode>(root_frame);
  }

  /// Get the root node
  std::shared_ptr<TreeNode> root() const noexcept { return root_; }

  /// Set the number of processes
  void set_process_count(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    process_count_ = count;
    root_->set_process_count(count);
  }

  /// Get the number of processes
  size_t process_count() const noexcept { return process_count_; }
  
  /// Get the tree build mode
  TreeBuildMode build_mode() const noexcept { return build_mode_; }
  
  /// Set the tree build mode (must be called before inserting call stacks)
  void set_build_mode(TreeBuildMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    build_mode_ = mode;
  }
  
  /// Set the sample count mode (must be called before inserting call stacks)
  void set_sample_count_mode(SampleCountMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    sample_count_mode_ = mode;
  }
  
  /// Get the sample count mode
  SampleCountMode sample_count_mode() const noexcept { return sample_count_mode_; }

  /// Insert a call stack into the tree
  /// @param frames Resolved frames from bottom (main) to top (leaf)
  /// @param process_id Process/rank ID
  /// @param count Sample count
  /// @param time_us Execution time in microseconds
  void insert_call_stack(const std::vector<sampling::ResolvedFrame>& frames,
                         size_t process_id, uint64_t count = 1,
                         double time_us = 0.0) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (frames.empty()) {
      return;
    }

    // Ensure process count is sufficient
    if (process_id >= process_count_) {
      set_process_count_nolock(process_id + 1);
    }

    // Add samples to root
    root_->add_sample(process_id, count, time_us);

    std::shared_ptr<TreeNode> current = root_;

    // Traverse from bottom to top (forward iteration since frames are already bottom-to-top)
    for (const auto& frame : frames) {
      // Try to find existing child with this frame
      // Use context-aware or context-free search based on build mode
      std::shared_ptr<TreeNode> child;
      if (build_mode_ == TreeBuildMode::kContextAware) {
        child = current->find_child_context_aware(frame);
      } else {
        child = current->find_child(frame);
      }
      
      if (!child) {
        // Create new child node
        child = std::make_shared<TreeNode>(frame);
        child->set_process_count(process_count_);
        current->add_child(child);
      }

      // Update call count edge
      current->increment_call_count(child, count);

      // Add samples based on sample count mode
      if (sample_count_mode_ == SampleCountMode::kInclusive || 
          sample_count_mode_ == SampleCountMode::kBoth) {
        // Track inclusive (total) samples - add to all nodes in path
        child->add_sample(process_id, count, time_us);
      }

      current = child;
    }

    // Mark self samples at the leaf based on sample count mode
    if (sample_count_mode_ == SampleCountMode::kExclusive || 
        sample_count_mode_ == SampleCountMode::kBoth) {
      // Track exclusive (self) samples - only at leaf node
      current->add_self_sample(count);
    }
    
    // For exclusive mode, also set total_samples at leaf to match self_samples
    if (sample_count_mode_ == SampleCountMode::kExclusive) {
      current->add_sample(process_id, count, time_us);
    }
  }

  /// Get total number of samples in the tree
  uint64_t total_samples() const noexcept {
    return root_ ? root_->total_samples() : 0;
  }

  /// Clear the tree
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    sampling::ResolvedFrame root_frame;
    root_frame.function_name = "[root]";
    root_frame.library_name = "[virtual]";
    root_ = std::make_shared<TreeNode>(root_frame);
    root_->set_process_count(process_count_);
  }

  // ============================================================================
  // Tree-based Data Access APIs
  // ============================================================================

  /// Get the total number of nodes in the tree
  size_t node_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_nodes_recursive(root_);
  }

  /// Get all nodes in the tree as a flat list
  std::vector<std::shared_ptr<TreeNode>> all_nodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    collect_nodes_recursive(root_, result);
    return result;
  }

  /// Get all leaf nodes (nodes with no children)
  std::vector<std::shared_ptr<TreeNode>> leaf_nodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    collect_leaf_nodes_recursive(root_, result);
    return result;
  }

  /// Find all nodes matching a function name
  std::vector<std::shared_ptr<TreeNode>> find_nodes_by_name(
      const std::string& func_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    find_nodes_by_name_recursive(root_, func_name, result);
    return result;
  }

  /// Find all nodes matching a library name
  std::vector<std::shared_ptr<TreeNode>> find_nodes_by_library(
      const std::string& lib_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    find_nodes_by_library_recursive(root_, lib_name, result);
    return result;
  }

  /// Traverse tree in pre-order (depth-first, parent before children)
  /// @param visitor Function called for each node, return false to stop traversal
  template <typename Visitor>
  void traverse_preorder(Visitor visitor) const {
    std::lock_guard<std::mutex> lock(mutex_);
    traverse_preorder_recursive(root_, visitor);
  }

  /// Traverse tree in post-order (depth-first, children before parent)
  /// @param visitor Function called for each node, return false to stop traversal
  template <typename Visitor>
  void traverse_postorder(Visitor visitor) const {
    std::lock_guard<std::mutex> lock(mutex_);
    traverse_postorder_recursive(root_, visitor);
  }

  /// Traverse tree level-by-level (breadth-first)
  /// @param visitor Function called for each node, return false to stop traversal
  template <typename Visitor>
  void traverse_levelorder(Visitor visitor) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> queue;
    queue.push_back(root_);
    size_t idx = 0;
    while (idx < queue.size()) {
      auto node = queue[idx++];
      if (!visitor(node)) {
        return;
      }
      for (const auto& child : node->children()) {
        queue.push_back(child);
      }
    }
  }

  /// Get nodes with samples above a threshold
  std::vector<std::shared_ptr<TreeNode>> filter_by_samples(
      uint64_t min_samples) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    filter_nodes_recursive(root_, [min_samples](const std::shared_ptr<TreeNode>& node) {
      return node->total_samples() >= min_samples;
    }, result);
    return result;
  }

  /// Get nodes with self samples above a threshold
  std::vector<std::shared_ptr<TreeNode>> filter_by_self_samples(
      uint64_t min_self_samples) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    filter_nodes_recursive(root_, [min_self_samples](const std::shared_ptr<TreeNode>& node) {
      return node->self_samples() >= min_self_samples;
    }, result);
    return result;
  }

  /// Get nodes at a specific depth
  std::vector<std::shared_ptr<TreeNode>> nodes_at_depth(size_t depth) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    collect_nodes_at_depth_recursive(root_, 0, depth, result);
    return result;
  }

  /// Get the maximum depth of the tree
  size_t max_depth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return calculate_max_depth_recursive(root_, 0);
  }

  /// Filter nodes using a custom predicate
  template <typename Predicate>
  std::vector<std::shared_ptr<TreeNode>> filter_nodes(Predicate pred) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<TreeNode>> result;
    filter_nodes_recursive(root_, pred, result);
    return result;
  }

 private:
  void set_process_count_nolock(size_t count) {
    process_count_ = count;
    root_->set_process_count(count);
  }

  static size_t count_nodes_recursive(const std::shared_ptr<TreeNode>& node) {
    if (!node) return 0;
    size_t count = 1;
    for (const auto& child : node->children()) {
      count += count_nodes_recursive(child);
    }
    return count;
  }

  static void collect_nodes_recursive(
      const std::shared_ptr<TreeNode>& node,
      std::vector<std::shared_ptr<TreeNode>>& result) {
    if (!node) return;
    result.push_back(node);
    for (const auto& child : node->children()) {
      collect_nodes_recursive(child, result);
    }
  }

  static void collect_leaf_nodes_recursive(
      const std::shared_ptr<TreeNode>& node,
      std::vector<std::shared_ptr<TreeNode>>& result) {
    if (!node) return;
    if (node->children().empty()) {
      result.push_back(node);
    } else {
      for (const auto& child : node->children()) {
        collect_leaf_nodes_recursive(child, result);
      }
    }
  }

  static void find_nodes_by_name_recursive(
      const std::shared_ptr<TreeNode>& node,
      const std::string& func_name,
      std::vector<std::shared_ptr<TreeNode>>& result) {
    if (!node) return;
    if (node->frame().function_name == func_name) {
      result.push_back(node);
    }
    for (const auto& child : node->children()) {
      find_nodes_by_name_recursive(child, func_name, result);
    }
  }

  static void find_nodes_by_library_recursive(
      const std::shared_ptr<TreeNode>& node,
      const std::string& lib_name,
      std::vector<std::shared_ptr<TreeNode>>& result) {
    if (!node) return;
    if (node->frame().library_name == lib_name) {
      result.push_back(node);
    }
    for (const auto& child : node->children()) {
      find_nodes_by_library_recursive(child, lib_name, result);
    }
  }

  template <typename Visitor>
  static bool traverse_preorder_recursive(
      const std::shared_ptr<TreeNode>& node, Visitor& visitor) {
    if (!node) return true;
    if (!visitor(node)) return false;
    for (const auto& child : node->children()) {
      if (!traverse_preorder_recursive(child, visitor)) return false;
    }
    return true;
  }

  template <typename Visitor>
  static bool traverse_postorder_recursive(
      const std::shared_ptr<TreeNode>& node, Visitor& visitor) {
    if (!node) return true;
    for (const auto& child : node->children()) {
      if (!traverse_postorder_recursive(child, visitor)) return false;
    }
    return visitor(node);
  }

  template <typename Predicate>
  static void filter_nodes_recursive(
      const std::shared_ptr<TreeNode>& node,
      Predicate pred,
      std::vector<std::shared_ptr<TreeNode>>& result) {
    if (!node) return;
    if (pred(node)) {
      result.push_back(node);
    }
    for (const auto& child : node->children()) {
      filter_nodes_recursive(child, pred, result);
    }
  }

  static void collect_nodes_at_depth_recursive(
      const std::shared_ptr<TreeNode>& node,
      size_t current_depth,
      size_t target_depth,
      std::vector<std::shared_ptr<TreeNode>>& result) {
    if (!node) return;
    if (current_depth == target_depth) {
      result.push_back(node);
      return;
    }
    for (const auto& child : node->children()) {
      collect_nodes_at_depth_recursive(child, current_depth + 1, target_depth, result);
    }
  }

  static size_t calculate_max_depth_recursive(
      const std::shared_ptr<TreeNode>& node,
      size_t current_depth) {
    if (!node) return current_depth;
    if (node->children().empty()) return current_depth;
    size_t max_child_depth = current_depth;
    for (const auto& child : node->children()) {
      size_t child_depth = calculate_max_depth_recursive(child, current_depth + 1);
      if (child_depth > max_child_depth) {
        max_child_depth = child_depth;
      }
    }
    return max_child_depth;
  }

  std::shared_ptr<TreeNode> root_;
  size_t process_count_;
  TreeBuildMode build_mode_;  // Tree building mode
  SampleCountMode sample_count_mode_;  // Sample counting mode
  mutable std::mutex mutex_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
