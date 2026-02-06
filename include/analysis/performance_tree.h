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
  
  /// Enable fine-grained locking (per-node mutexes)
  /// @param enable True to enable, false to disable
  void enable_fine_grained_locking(bool enable) {
    use_fine_grained_locking_ = enable;
  }
  
  /// Enable lock-free operations (atomic counters)
  /// @param enable True to enable, false to disable
  void enable_lock_free(bool enable) {
    use_lock_free_ = enable;
  }
  
  /// Merge another tree into this tree
  /// @param other The tree to merge from
  void merge(const PerformanceTree& other) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!other.root_) {
      return;
    }
    
    // Ensure process counts match
    if (other.process_count_ > process_count_) {
      set_process_count_nolock(other.process_count_);
    }
    
    // Recursively merge starting from children of root
    for (const auto& child : other.root_->children()) {
      merge_node(root_, child);
    }
  }

 private:
  void set_process_count_nolock(size_t count) {
    process_count_ = count;
    root_->set_process_count(count);
  }
  
  /// Recursively merge a node from another tree into this tree
  void merge_node(std::shared_ptr<TreeNode> parent,
                  const std::shared_ptr<TreeNode>& source_node) {
    // Find or create matching child
    std::shared_ptr<TreeNode> target_node;
    if (build_mode_ == TreeBuildMode::kContextAware) {
      target_node = parent->find_child_context_aware(source_node->frame());
    } else {
      target_node = parent->find_child(source_node->frame());
    }
    
    if (!target_node) {
      // Create new node
      target_node = std::make_shared<TreeNode>(source_node->frame());
      target_node->set_process_count(process_count_);
      parent->add_child(target_node);
    }
    
    // Merge sample counts and execution times
    const auto& src_counts = source_node->sampling_counts();
    const auto& src_times = source_node->execution_times();
    for (size_t i = 0; i < src_counts.size() && i < process_count_; ++i) {
      if (src_counts[i] > 0 || src_times[i] > 0.0) {
        target_node->add_sample(i, src_counts[i], src_times[i]);
      }
    }
    
    // Merge self samples
    if (source_node->self_samples() > 0) {
      target_node->add_self_sample(source_node->self_samples());
    }
    
    // Recursively merge children
    for (const auto& child : source_node->children()) {
      merge_node(target_node, child);
    }
  }

  std::shared_ptr<TreeNode> root_;
  size_t process_count_;
  TreeBuildMode build_mode_;  // Tree building mode
  SampleCountMode sample_count_mode_;  // Sample counting mode
  mutable std::mutex mutex_;
  bool use_fine_grained_locking_ = false;
  bool use_lock_free_ = false;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
