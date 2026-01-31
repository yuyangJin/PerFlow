// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
#define PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "sampling/call_stack.h"

namespace perflow {
namespace analysis {

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
  /// Constructor
  PerformanceTree() noexcept : root_(nullptr), process_count_(0), mutex_() {
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

    std::shared_ptr<TreeNode> current = root_;

    // Traverse from bottom to top (reverse iteration for top-down tree)
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
      const auto& frame = *it;

      // Try to find existing child with this frame
      auto child = current->find_child(frame);
      if (!child) {
        // Create new child node
        child = std::make_shared<TreeNode>(frame);
        child->set_process_count(process_count_);
        current->add_child(child);
      }

      // Update call count edge
      current->increment_call_count(child, count);

      // Add sample to this node
      child->add_sample(process_id, count, time_us);

      current = child;
    }

    // Mark self samples at the leaf
    current->add_self_sample(count);
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

 private:
  void set_process_count_nolock(size_t count) {
    process_count_ = count;
    root_->set_process_count(count);
  }

  std::shared_ptr<TreeNode> root_;
  size_t process_count_;
  mutable std::mutex mutex_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
