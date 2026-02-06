// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_TREE_BUILDER_H_
#define PERFLOW_ANALYSIS_TREE_BUILDER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "analysis/offset_converter.h"
#include "analysis/performance_tree.h"
#include "sampling/call_stack.h"
#include "sampling/data_export.h"

namespace perflow {
namespace analysis {

/// Parallel build strategy for multi-threaded tree construction
enum class ParallelBuildStrategy {
  /// Coarse-grained locking: Single mutex for entire tree (simplest, highest contention)
  /// Best for: Few threads, low contention scenarios
  /// Cost: O(1) lock per insert, serializes all inserts
  kCoarseLock = 0,
  
  /// Fine-grained locking: Per-node mutexes (medium contention)
  /// Best for: Medium thread counts, mixed workloads
  /// Cost: O(depth) locks per insert, allows parallel inserts to different subtrees
  kFineGrainedLock = 1,
  
  /// Thread-local trees with merge: Each thread builds own tree, merge at end (lowest contention)
  /// Best for: High thread counts, overlapping call stacks
  /// Cost: O(1) during build, O(nodes) for merge, excellent for high parallelism
  kThreadLocalMerge = 2,
  
  /// Lock-free with atomics: Atomic counters, locks only for structure changes (lowest overhead)
  /// Best for: High contention, mostly updates to existing nodes
  /// Cost: Lock-free for counter updates, locks only for new node creation
  kLockFree = 3
};

/// TreeBuilder constructs performance trees from sample data
class TreeBuilder {
 public:
  /// Constructor with optional build mode and sample count mode
  explicit TreeBuilder(TreeBuildMode mode = TreeBuildMode::kContextFree,
                      SampleCountMode count_mode = SampleCountMode::kExclusive) noexcept 
      : converter_(), tree_(mode, count_mode), parallel_strategy_(ParallelBuildStrategy::kCoarseLock) {}

  /// Get the performance tree
  PerformanceTree& tree() noexcept { return tree_; }
  const PerformanceTree& tree() const noexcept { return tree_; }

  /// Get the offset converter
  OffsetConverter& converter() noexcept { return converter_; }
  const OffsetConverter& converter() const noexcept { return converter_; }
  
  /// Set the parallel build strategy
  void set_parallel_strategy(ParallelBuildStrategy strategy) {
    parallel_strategy_ = strategy;
  }
  
  /// Get the parallel build strategy
  ParallelBuildStrategy parallel_strategy() const noexcept {
    return parallel_strategy_;
  }
  
  /// Set the tree build mode (must be called before building)
  void set_build_mode(TreeBuildMode mode) {
    tree_.set_build_mode(mode);
  }
  
  /// Get the tree build mode
  TreeBuildMode build_mode() const noexcept {
    return tree_.build_mode();
  }
  
  /// Set the sample count mode (must be called before building)
  void set_sample_count_mode(SampleCountMode mode) {
    tree_.set_sample_count_mode(mode);
  }
  
  /// Get the sample count mode
  SampleCountMode sample_count_mode() const noexcept {
    return tree_.sample_count_mode();
  }

  /// Build tree from a sample data file
  /// @param sample_file Path to sample data file (.pflw)
  /// @param process_id Process/rank ID for this file
  /// @param time_per_sample Estimated time per sample in microseconds
  /// @return True if successful, false otherwise
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth,
            size_t Capacity = 1048576>
  bool build_from_file(const char* sample_file, uint32_t process_id,
                       double time_per_sample = 1000.0) {
    // Import sample data - allocate on heap to avoid stack overflow
    auto data = std::make_unique<sampling::StaticHashMap<sampling::CallStack<MaxDepth>, uint64_t, Capacity>>();
    sampling::DataImporter importer(sample_file);
    auto result = importer.importData(*data);

    if (result != sampling::DataResult::kSuccess) {
      return false;
    }

    // Convert and insert each call stack
    bool has_converter = converter_.has_snapshot(process_id);

    // Store references to avoid issues in lambda
    PerformanceTree& tree_ref = tree_;
    OffsetConverter& converter_ref = converter_;

    data->for_each([&tree_ref, &converter_ref, process_id, time_per_sample, has_converter](
                      const sampling::CallStack<MaxDepth>& stack,
                      const uint64_t& count) {
      std::vector<sampling::ResolvedFrame> frames;

      if (has_converter) {
        // Convert using offset converter with symbol resolution if available
        bool resolve_symbols = converter_ref.has_symbol_resolver();
        frames = converter_ref.convert(stack, process_id, resolve_symbols);
      } else {
        // Create basic frames with raw addresses
        for (size_t i = 0; i < stack.depth(); ++i) {
          sampling::ResolvedFrame frame;
          frame.raw_address = stack.frame(i);
          frame.offset = stack.frame(i);
          frame.library_name = "[unknown]";
          frame.function_name = "[unknown]";
          frames.push_back(frame);
        }
      }

      // Insert into tree
      double total_time = count * time_per_sample;
      tree_ref.insert_call_stack(frames, process_id, count, total_time);
    });

    return true;
  }

  /// Build tree from multiple sample files
  /// @param sample_files Vector of (file_path, process_id) pairs
  /// @param time_per_sample Estimated time per sample in microseconds
  /// @return Number of files successfully processed
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth,
            size_t Capacity = 1048576>
  size_t build_from_files(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample = 1000.0) {
    size_t success_count = 0;

    // Determine total process count
    size_t max_process_id = 0;
    for (const auto& pair : sample_files) {
      if (pair.second > max_process_id) {
        max_process_id = pair.second;
      }
    }
    tree_.set_process_count(max_process_id + 1);

    // Build from each file
    for (const auto& pair : sample_files) {
      if (build_from_file<MaxDepth, Capacity>(pair.first.c_str(), pair.second,
                                               time_per_sample)) {
        ++success_count;
      }
    }

    return success_count;
  }
  
  /// Build tree from multiple sample files in parallel
  /// @param sample_files Vector of (file_path, process_id) pairs
  /// @param time_per_sample Estimated time per sample in microseconds
  /// @param num_threads Number of threads to use (0 = auto)
  /// @return Number of files successfully processed
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth,
            size_t Capacity = 1048576>
  size_t build_from_files_parallel(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample = 1000.0,
      size_t num_threads = 0) {
    if (sample_files.empty()) {
      return 0;
    }
    
    // Determine total process count
    size_t max_process_id = 0;
    for (const auto& pair : sample_files) {
      if (pair.second > max_process_id) {
        max_process_id = pair.second;
      }
    }
    tree_.set_process_count(max_process_id + 1);
    
    // Determine number of threads
    size_t threads = num_threads;
    if (threads == 0) {
      threads = std::thread::hardware_concurrency();
      if (threads == 0) threads = 4;  // Fallback
    }
    threads = std::min(threads, sample_files.size());
    
    // Dispatch to appropriate strategy
    switch (parallel_strategy_) {
      case ParallelBuildStrategy::kCoarseLock:
        return build_parallel_coarse_lock<MaxDepth, Capacity>(
            sample_files, time_per_sample, threads);
            
      case ParallelBuildStrategy::kFineGrainedLock:
        tree_.enable_fine_grained_locking(true);
        return build_parallel_fine_grained<MaxDepth, Capacity>(
            sample_files, time_per_sample, threads);
            
      case ParallelBuildStrategy::kThreadLocalMerge:
        return build_parallel_thread_local<MaxDepth, Capacity>(
            sample_files, time_per_sample, threads);
            
      case ParallelBuildStrategy::kLockFree:
        tree_.enable_lock_free(true);
        return build_parallel_lock_free<MaxDepth, Capacity>(
            sample_files, time_per_sample, threads);
            
      default:
        return build_parallel_coarse_lock<MaxDepth, Capacity>(
            sample_files, time_per_sample, threads);
    }
  }

 private:
  /// Strategy 1: Coarse-grained locking (current implementation)
  template <size_t MaxDepth, size_t Capacity>
  size_t build_parallel_coarse_lock(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample,
      size_t threads) {
    std::vector<std::thread> workers;
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> next_file_idx{0};
    
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back([&]() {
        while (true) {
          size_t idx = next_file_idx.fetch_add(1);
          if (idx >= sample_files.size()) {
            break;
          }
          
          const auto& [file_path, process_id] = sample_files[idx];
          if (build_from_file<MaxDepth, Capacity>(file_path.c_str(), 
                                                   process_id,
                                                   time_per_sample)) {
            success_count.fetch_add(1);
          }
        }
      });
    }
    
    for (auto& worker : workers) {
      worker.join();
    }
    
    return success_count.load();
  }
  
  /// Strategy 2: Fine-grained locking
  template <size_t MaxDepth, size_t Capacity>
  size_t build_parallel_fine_grained(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample,
      size_t threads) {
    // Same as coarse lock but tree uses per-node locking internally
    return build_parallel_coarse_lock<MaxDepth, Capacity>(
        sample_files, time_per_sample, threads);
  }
  
  /// Strategy 3: Thread-local trees with merge
  template <size_t MaxDepth, size_t Capacity>
  size_t build_parallel_thread_local(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample,
      size_t threads) {
    std::vector<std::thread> workers;
    std::atomic<size_t> next_file_idx{0};
    
    // Create thread-local trees
    std::vector<PerformanceTree> local_trees(threads, 
        PerformanceTree(tree_.build_mode(), tree_.sample_count_mode()));
    std::vector<size_t> local_success_counts(threads, 0);
    
    for (size_t thread_id = 0; thread_id < threads; ++thread_id) {
      workers.emplace_back([&, thread_id]() {
        auto& local_tree = local_trees[thread_id];
        local_tree.set_process_count(tree_.process_count());
        
        while (true) {
          size_t idx = next_file_idx.fetch_add(1);
          if (idx >= sample_files.size()) {
            break;
          }
          
          const auto& [file_path, process_id] = sample_files[idx];
          
          // Build into local tree
          auto data = std::make_unique<sampling::StaticHashMap<
              sampling::CallStack<MaxDepth>, uint64_t, Capacity>>();
          sampling::DataImporter importer(file_path.c_str());
          auto result = importer.importData(*data);
          
          if (result != sampling::DataResult::kSuccess) {
            continue;
          }
          
          bool has_converter = converter_.has_snapshot(process_id);
          data->for_each([&](const sampling::CallStack<MaxDepth>& stack,
                            const uint64_t& count) {
            std::vector<sampling::ResolvedFrame> frames;
            
            if (has_converter) {
              bool resolve_symbols = converter_.has_symbol_resolver();
              frames = converter_.convert(stack, process_id, resolve_symbols);
            } else {
              for (size_t i = 0; i < stack.depth(); ++i) {
                sampling::ResolvedFrame frame;
                frame.raw_address = stack.frame(i);
                frame.offset = stack.frame(i);
                frame.library_name = "[unknown]";
                frame.function_name = "[unknown]";
                frames.push_back(frame);
              }
            }
            
            double total_time = count * time_per_sample;
            local_tree.insert_call_stack(frames, process_id, count, total_time);
          });
          
          local_success_counts[thread_id]++;
        }
      });
    }
    
    for (auto& worker : workers) {
      worker.join();
    }
    
    // Merge all local trees into main tree
    for (auto& local_tree : local_trees) {
      tree_.merge(local_tree);
    }
    
    size_t total_success = 0;
    for (size_t count : local_success_counts) {
      total_success += count;
    }
    
    return total_success;
  }
  
  /// Strategy 4: Lock-free with atomics
  template <size_t MaxDepth, size_t Capacity>
  size_t build_parallel_lock_free(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample,
      size_t threads) {
    // Same as coarse lock but tree uses atomic operations internally
    return build_parallel_coarse_lock<MaxDepth, Capacity>(
        sample_files, time_per_sample, threads);
  }

 public:

  /// Load library maps for address resolution
  /// @param libmap_files Vector of (file_path, process_id) pairs
  /// @return Number of maps successfully loaded
  size_t load_library_maps(
      const std::vector<std::pair<std::string, uint32_t>>& libmap_files) {
    size_t success_count = 0;

    for (const auto& pair : libmap_files) {
      sampling::LibraryMap lib_map;
      sampling::LibraryMapImporter importer(pair.first.c_str());
      uint32_t pid;
      auto result = importer.importMap(lib_map, &pid);

      if (result == sampling::DataResult::kSuccess) {
        converter_.add_map_snapshot(pair.second, lib_map);
        ++success_count;
      }
    }

    return success_count;
  }

  /// Clear all data
  void clear() {
    tree_.clear();
    converter_.clear();
  }

 private:
  OffsetConverter converter_;
  PerformanceTree tree_;
  ParallelBuildStrategy parallel_strategy_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_TREE_BUILDER_H_
