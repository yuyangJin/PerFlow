// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
#define PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

/// ConcurrencyModel defines how concurrent tree building is handled
/// This determines the synchronization strategy for parallel insertions
enum class ConcurrencyModel {
  /// Serial mode: Single global lock for all operations
  /// Simple and safe, but limits scalability
  /// Expected cost: O(1) lock per operation, but no parallelism
  kSerial = 0,
  
  /// Fine-grained locking: Each tree node has its own lock
  /// Nodes are locked individually during updates
  /// Allows parallel subtree insertion
  /// Expected cost: O(depth) locks per operation
  kFineGrainedLock = 1,
  
  /// Thread-local merge: Each thread builds a separate tree
  /// Trees are merged after construction
  /// No contention during build phase
  /// Expected cost: O(1) during build + O(nodes) for single-threaded merge
  kThreadLocalMerge = 2,
  
  /// Lock-free: Use atomic operations for node attributes
  /// Only use locks for structural changes (adding new nodes)
  /// Minimizes contention on existing nodes
  /// Expected cost: Lock-free counter updates
  kLockFree = 3
};

/// TreeNode represents a vertex in the performance tree
/// Each node corresponds to a function/program structure
/// Supports multiple concurrency models for parallel tree building
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
        self_samples_(0),
        atomic_total_samples_(0),
        atomic_self_samples_(0),
        atomic_capacity_(0),
        node_mutex_() {}

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
    // Reallocate atomic arrays if needed
    if (count > atomic_capacity_) {
      auto new_sampling = std::make_unique<std::atomic<uint64_t>[]>(count);
      auto new_times = std::make_unique<std::atomic<double>[]>(count);
      // Copy old values if any exist
      for (size_t i = 0; i < atomic_capacity_; ++i) {
        new_sampling[i].store(atomic_sampling_counts_[i].load(std::memory_order_relaxed), 
                             std::memory_order_relaxed);
        new_times[i].store(atomic_execution_times_[i].load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
      }
      // Initialize new elements
      for (size_t i = atomic_capacity_; i < count; ++i) {
        new_sampling[i].store(0, std::memory_order_relaxed);
        new_times[i].store(0.0, std::memory_order_relaxed);
      }
      atomic_sampling_counts_ = std::move(new_sampling);
      atomic_execution_times_ = std::move(new_times);
      atomic_capacity_ = count;
    }
  }

  /// Add a sample for a specific process (non-atomic, for serial/fine-grained modes)
  void add_sample(size_t process_id, uint64_t count = 1,
                  double time_us = 0.0) {
    if (process_id >= sampling_counts_.size()) {
      set_process_count(process_id + 1);
    }
    sampling_counts_[process_id] += count;
    execution_times_[process_id] += time_us;
    total_samples_ += count;
  }

  /// Add a sample atomically (for lock-free mode)
  void add_sample_atomic(size_t process_id, uint64_t count = 1,
                         double time_us = 0.0) {
    // Check if resize is needed and acquire lock if so
    // After resize, re-check capacity to handle concurrent resize
    if (process_id >= atomic_capacity_) {
      std::lock_guard<std::mutex> lock(node_mutex_);
      // Double-check after acquiring lock
      if (process_id >= atomic_capacity_) {
        set_process_count(process_id + 1);
      }
    }
    
    // Now safe to access - capacity is monotonically increasing
    atomic_sampling_counts_[process_id].fetch_add(count, std::memory_order_relaxed);
    // For execution time, we need CAS loop since there's no atomic add for double
    double old_val = atomic_execution_times_[process_id].load(std::memory_order_relaxed);
    while (!atomic_execution_times_[process_id].compare_exchange_weak(
        old_val, old_val + time_us, std::memory_order_relaxed, std::memory_order_relaxed)) {
      // Retry
    }
    atomic_total_samples_.fetch_add(count, std::memory_order_relaxed);
  }

  /// Add self samples (leaf node samples)
  void add_self_sample(uint64_t count = 1) { self_samples_ += count; }

  /// Add self samples atomically (for lock-free mode)
  void add_self_sample_atomic(uint64_t count = 1) {
    atomic_self_samples_.fetch_add(count, std::memory_order_relaxed);
  }

  /// Get total samples across all processes
  uint64_t total_samples() const noexcept { return total_samples_; }

  /// Get total samples atomically (for lock-free mode)
  uint64_t total_samples_atomic() const noexcept {
    return atomic_total_samples_.load(std::memory_order_relaxed);
  }

  /// Get self samples (samples at this leaf)
  uint64_t self_samples() const noexcept { return self_samples_; }

  /// Get self samples atomically (for lock-free mode)
  uint64_t self_samples_atomic() const noexcept {
    return atomic_self_samples_.load(std::memory_order_relaxed);
  }

  /// Consolidate atomic counters to regular counters (call after parallel phase)
  void consolidate_atomic_counters() {
    total_samples_ = atomic_total_samples_.load(std::memory_order_relaxed);
    self_samples_ = atomic_self_samples_.load(std::memory_order_relaxed);
    if (atomic_sampling_counts_) {
      for (size_t i = 0; i < atomic_capacity_ && i < sampling_counts_.size(); ++i) {
        sampling_counts_[i] = atomic_sampling_counts_[i].load(std::memory_order_relaxed);
      }
    }
    if (atomic_execution_times_) {
      for (size_t i = 0; i < atomic_capacity_ && i < execution_times_.size(); ++i) {
        execution_times_[i] = atomic_execution_times_[i].load(std::memory_order_relaxed);
      }
    }
  }

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

  /// Add a child node with lock (for fine-grained and lock-free modes)
  void add_child_locked(std::shared_ptr<TreeNode> child) {
    std::lock_guard<std::mutex> lock(node_mutex_);
    child->parent_ = this;
    children_.push_back(child);
  }

  /// Get the per-node mutex (for fine-grained locking)
  std::mutex& node_mutex() { return node_mutex_; }

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
  
  /// Find a child with lock (for lock-free and fine-grained modes)
  std::shared_ptr<TreeNode> find_child_locked(
      const sampling::ResolvedFrame& frame) {
    std::lock_guard<std::mutex> lock(node_mutex_);
    return find_child(frame);
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

  /// Find a child context-aware with lock (for lock-free and fine-grained modes)
  std::shared_ptr<TreeNode> find_child_context_aware_locked(
      const sampling::ResolvedFrame& frame) {
    std::lock_guard<std::mutex> lock(node_mutex_);
    return find_child_context_aware(frame);
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

  /// Increment call count atomically (for lock-free mode)
  void increment_call_count_locked(const std::shared_ptr<TreeNode>& child,
                                   uint64_t count = 1) {
    std::lock_guard<std::mutex> lock(node_mutex_);
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
  
  // Atomic counters for lock-free concurrency model
  std::unique_ptr<std::atomic<uint64_t>[]> atomic_sampling_counts_;
  std::unique_ptr<std::atomic<double>[]> atomic_execution_times_;
  std::atomic<uint64_t> atomic_total_samples_;
  std::atomic<uint64_t> atomic_self_samples_;
  size_t atomic_capacity_;  // Current capacity of atomic arrays
  
  // Per-node mutex for fine-grained locking model
  mutable std::mutex node_mutex_;
};

/// PerformanceTree aggregates call stack samples into a tree structure
/// Thread-safe for concurrent insertions with configurable concurrency models
class PerformanceTree {
 public:
  /// Constructor with optional build mode, sample count mode, and concurrency model
  explicit PerformanceTree(TreeBuildMode mode = TreeBuildMode::kContextFree,
                          SampleCountMode count_mode = SampleCountMode::kExclusive,
                          ConcurrencyModel concurrency_model = ConcurrencyModel::kSerial) noexcept 
      : root_(nullptr), process_count_(0), build_mode_(mode), 
        sample_count_mode_(count_mode), concurrency_model_(concurrency_model),
        num_threads_(std::thread::hardware_concurrency()),
        mutex_() {
    if (num_threads_ == 0) num_threads_ = 1;
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
  
  /// Get the concurrency model
  ConcurrencyModel concurrency_model() const noexcept { return concurrency_model_; }
  
  /// Set the concurrency model (must be called before inserting call stacks)
  void set_concurrency_model(ConcurrencyModel model) {
    std::lock_guard<std::mutex> lock(mutex_);
    concurrency_model_ = model;
  }
  
  /// Get the number of threads for parallel operations
  size_t num_threads() const noexcept { return num_threads_; }
  
  /// Set the number of threads for parallel operations
  void set_num_threads(size_t num_threads) {
    std::lock_guard<std::mutex> lock(mutex_);
    num_threads_ = num_threads > 0 ? num_threads : 1;
  }

  /// Insert a call stack into the tree
  /// @param frames Resolved frames from bottom (main) to top (leaf)
  /// @param process_id Process/rank ID
  /// @param count Sample count
  /// @param time_us Execution time in microseconds
  void insert_call_stack(const std::vector<sampling::ResolvedFrame>& frames,
                         size_t process_id, uint64_t count = 1,
                         double time_us = 0.0) {
    switch (concurrency_model_) {
      case ConcurrencyModel::kSerial:
        insert_call_stack_serial(frames, process_id, count, time_us);
        break;
      case ConcurrencyModel::kFineGrainedLock:
        insert_call_stack_fine_grained(frames, process_id, count, time_us);
        break;
      case ConcurrencyModel::kLockFree:
        insert_call_stack_lock_free(frames, process_id, count, time_us);
        break;
      case ConcurrencyModel::kThreadLocalMerge:
        // For thread-local merge, the caller should use create_thread_local_tree()
        // and merge_thread_local_tree() instead. Fall back to serial for direct calls.
        insert_call_stack_serial(frames, process_id, count, time_us);
        break;
    }
  }
  
  /// Create a thread-local tree for the ThreadLocalMerge concurrency model
  /// @return A new PerformanceTree that can be used independently by a single thread
  std::unique_ptr<PerformanceTree> create_thread_local_tree() const {
    auto tree = std::make_unique<PerformanceTree>(build_mode_, sample_count_mode_, ConcurrencyModel::kSerial);
    tree->set_process_count(process_count_);
    return tree;
  }
  
  /// Merge a thread-local tree into this tree (for ThreadLocalMerge model)
  /// This operation is NOT thread-safe - call only from a single thread after parallel phase
  /// @param other The thread-local tree to merge
  void merge_thread_local_tree(const PerformanceTree& other) {
    std::lock_guard<std::mutex> lock(mutex_);
    merge_node_recursive(other.root_, root_, std::vector<sampling::ResolvedFrame>());
  }
  
  /// Consolidate atomic counters after parallel insertion (for lock-free model)
  /// Call this after all parallel insertions are complete
  void consolidate_atomic_counters() {
    std::lock_guard<std::mutex> lock(mutex_);
    consolidate_node_recursive(root_);
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

  // ==========================================================================
  // Serial Concurrency Model Implementation
  // ==========================================================================
  
  /// Insert call stack with global lock (Serial mode)
  void insert_call_stack_serial(const std::vector<sampling::ResolvedFrame>& frames,
                                size_t process_id, uint64_t count, double time_us) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (frames.empty()) {
      return;
    }

    if (process_id >= process_count_) {
      set_process_count_nolock(process_id + 1);
    }

    root_->add_sample(process_id, count, time_us);
    std::shared_ptr<TreeNode> current = root_;

    for (const auto& frame : frames) {
      std::shared_ptr<TreeNode> child;
      if (build_mode_ == TreeBuildMode::kContextAware) {
        child = current->find_child_context_aware(frame);
      } else {
        child = current->find_child(frame);
      }
      
      if (!child) {
        child = std::make_shared<TreeNode>(frame);
        child->set_process_count(process_count_);
        current->add_child(child);
      }

      current->increment_call_count(child, count);

      if (sample_count_mode_ == SampleCountMode::kInclusive || 
          sample_count_mode_ == SampleCountMode::kBoth) {
        child->add_sample(process_id, count, time_us);
      }

      current = child;
    }

    if (sample_count_mode_ == SampleCountMode::kExclusive || 
        sample_count_mode_ == SampleCountMode::kBoth) {
      current->add_self_sample(count);
    }
    
    if (sample_count_mode_ == SampleCountMode::kExclusive) {
      current->add_sample(process_id, count, time_us);
    }
  }

  // ==========================================================================
  // Fine-Grained Lock Concurrency Model Implementation
  // ==========================================================================
  
  /// Insert call stack with per-node locks (Fine-grained lock mode)
  /// Each node is locked individually, allowing parallel insertions to different subtrees
  void insert_call_stack_fine_grained(const std::vector<sampling::ResolvedFrame>& frames,
                                      size_t process_id, uint64_t count, double time_us) {
    if (frames.empty()) {
      return;
    }

    // Lock for process count check
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (process_id >= process_count_) {
        set_process_count_nolock(process_id + 1);
      }
    }

    // Lock root and add samples
    {
      std::lock_guard<std::mutex> lock(root_->node_mutex());
      root_->add_sample(process_id, count, time_us);
    }

    std::shared_ptr<TreeNode> current = root_;

    for (const auto& frame : frames) {
      std::shared_ptr<TreeNode> child;
      
      // Lock current node to find/create child
      {
        std::lock_guard<std::mutex> lock(current->node_mutex());
        
        if (build_mode_ == TreeBuildMode::kContextAware) {
          child = current->find_child_context_aware(frame);
        } else {
          child = current->find_child(frame);
        }
        
        if (!child) {
          child = std::make_shared<TreeNode>(frame);
          child->set_process_count(process_count_);
          current->add_child(child);
        }

        current->increment_call_count(child, count);
      }

      // Lock child node to update samples
      if (sample_count_mode_ == SampleCountMode::kInclusive || 
          sample_count_mode_ == SampleCountMode::kBoth) {
        std::lock_guard<std::mutex> lock(child->node_mutex());
        child->add_sample(process_id, count, time_us);
      }

      current = child;
    }

    // Update leaf node
    std::lock_guard<std::mutex> lock(current->node_mutex());
    if (sample_count_mode_ == SampleCountMode::kExclusive || 
        sample_count_mode_ == SampleCountMode::kBoth) {
      current->add_self_sample(count);
    }
    
    if (sample_count_mode_ == SampleCountMode::kExclusive) {
      current->add_sample(process_id, count, time_us);
    }
  }

  // ==========================================================================
  // Lock-Free Concurrency Model Implementation
  // ==========================================================================
  
  /// Insert call stack with atomic operations (Lock-free mode)
  /// Uses atomic operations for counter updates, locks only for structural changes
  void insert_call_stack_lock_free(const std::vector<sampling::ResolvedFrame>& frames,
                                   size_t process_id, uint64_t count, double time_us) {
    if (frames.empty()) {
      return;
    }

    // Lock for process count check
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (process_id >= process_count_) {
        set_process_count_nolock(process_id + 1);
      }
    }

    // Update root atomically
    root_->add_sample_atomic(process_id, count, time_us);

    std::shared_ptr<TreeNode> current = root_;

    for (const auto& frame : frames) {
      std::shared_ptr<TreeNode> child;
      
      // Try to find child without lock first (optimistic read)
      // This is safe because:
      // 1. If child is found, it's a valid shared_ptr that won't be removed
      // 2. If child is not found, we'll take a lock and double-check
      // 3. The children vector only grows (no removal), so once a child exists it stays
      if (build_mode_ == TreeBuildMode::kContextAware) {
        child = current->find_child_context_aware(frame);
      } else {
        child = current->find_child(frame);
      }
      
      if (!child) {
        // Need lock for structural change (adding new node)
        std::lock_guard<std::mutex> lock(current->node_mutex());
        
        // Double-check after acquiring lock - another thread may have added it
        if (build_mode_ == TreeBuildMode::kContextAware) {
          child = current->find_child_context_aware(frame);
        } else {
          child = current->find_child(frame);
        }
        
        if (!child) {
          child = std::make_shared<TreeNode>(frame);
          child->set_process_count(process_count_);
          current->add_child(child);
        }
      }

      // Update call count with lock (map modification)
      current->increment_call_count_locked(child, count);

      // Update samples atomically
      if (sample_count_mode_ == SampleCountMode::kInclusive || 
          sample_count_mode_ == SampleCountMode::kBoth) {
        child->add_sample_atomic(process_id, count, time_us);
      }

      current = child;
    }

    // Update leaf node atomically
    if (sample_count_mode_ == SampleCountMode::kExclusive || 
        sample_count_mode_ == SampleCountMode::kBoth) {
      current->add_self_sample_atomic(count);
    }
    
    if (sample_count_mode_ == SampleCountMode::kExclusive) {
      current->add_sample_atomic(process_id, count, time_us);
    }
  }

  // ==========================================================================
  // Thread-Local Merge Helper Methods
  // ==========================================================================
  
  /// Recursively merge nodes from source tree into destination tree
  void merge_node_recursive(
      const std::shared_ptr<TreeNode>& source,
      std::shared_ptr<TreeNode>& dest,
      std::vector<sampling::ResolvedFrame> current_path) {
    
    if (!source) return;

    // Skip root node frame but process its children
    if (source->frame().function_name != "[root]") {
      current_path.push_back(source->frame());
      
      // If this is a leaf, insert the call stack
      if (source->is_leaf()) {
        const auto& counts = source->sampling_counts();
        const auto& times = source->execution_times();
        
        for (size_t pid = 0; pid < counts.size(); ++pid) {
          if (counts[pid] > 0) {
            insert_call_stack_into_tree_nolock(
                current_path, pid, counts[pid],
                times.size() > pid ? times[pid] : 0.0);
          }
        }
        return;
      }
    }

    // Recurse to children
    for (const auto& child : source->children()) {
      merge_node_recursive(child, dest, current_path);
    }
  }

  /// Insert call stack directly without taking lock (for merge operations)
  void insert_call_stack_into_tree_nolock(
      const std::vector<sampling::ResolvedFrame>& frames,
      size_t process_id, uint64_t count, double time_us) {
    
    if (frames.empty()) {
      return;
    }

    root_->add_sample(process_id, count, time_us);
    std::shared_ptr<TreeNode> current = root_;

    for (const auto& frame : frames) {
      std::shared_ptr<TreeNode> child;
      if (build_mode_ == TreeBuildMode::kContextAware) {
        child = current->find_child_context_aware(frame);
      } else {
        child = current->find_child(frame);
      }
      
      if (!child) {
        child = std::make_shared<TreeNode>(frame);
        child->set_process_count(process_count_);
        current->add_child(child);
      }

      current->increment_call_count(child, count);

      if (sample_count_mode_ == SampleCountMode::kInclusive || 
          sample_count_mode_ == SampleCountMode::kBoth) {
        child->add_sample(process_id, count, time_us);
      }

      current = child;
    }

    if (sample_count_mode_ == SampleCountMode::kExclusive || 
        sample_count_mode_ == SampleCountMode::kBoth) {
      current->add_self_sample(count);
    }
    
    if (sample_count_mode_ == SampleCountMode::kExclusive) {
      current->add_sample(process_id, count, time_us);
    }
  }

  /// Recursively consolidate atomic counters to regular counters
  void consolidate_node_recursive(const std::shared_ptr<TreeNode>& node) {
    if (!node) return;
    
    node->consolidate_atomic_counters();
    
    for (const auto& child : node->children()) {
      consolidate_node_recursive(child);
    }
  }

  // ==========================================================================
  // Existing Helper Methods
  // ==========================================================================

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
  ConcurrencyModel concurrency_model_;  // Concurrency model for parallel operations
  size_t num_threads_;  // Number of threads for parallel operations
  mutable std::mutex mutex_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_PERFORMANCE_TREE_H_
