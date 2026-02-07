// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_TREE_BUILDER_H_
#define PERFLOW_ANALYSIS_TREE_BUILDER_H_

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "analysis/offset_converter.h"
#include "analysis/performance_tree.h"
#include "analysis/symbol_resolver.h"
#include "sampling/call_stack.h"
#include "sampling/data_export.h"

namespace perflow {
namespace analysis {

/// TreeBuilder constructs performance trees from sample data
/// Supports multiple concurrency models for parallel tree building
class TreeBuilder {
 public:
  /// Constructor with optional build mode, sample count mode, and concurrency model
  explicit TreeBuilder(TreeBuildMode mode = TreeBuildMode::kContextFree,
                      SampleCountMode count_mode = SampleCountMode::kExclusive,
                      ConcurrencyModel concurrency_model = ConcurrencyModel::kSerial) noexcept 
      : converter_(std::make_shared<SymbolResolver>()), 
        tree_(mode, count_mode, concurrency_model),
        num_threads_(std::thread::hardware_concurrency()) {
    if (num_threads_ == 0) num_threads_ = 1;
  }

  /// Get the performance tree
  PerformanceTree& tree() noexcept { return tree_; }
  const PerformanceTree& tree() const noexcept { return tree_; }

  /// Get the offset converter
  OffsetConverter& converter() noexcept { return converter_; }
  const OffsetConverter& converter() const noexcept { return converter_; }
  
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
  
  /// Set the concurrency model (must be called before building)
  void set_concurrency_model(ConcurrencyModel model) {
    tree_.set_concurrency_model(model);
  }
  
  /// Get the concurrency model
  ConcurrencyModel concurrency_model() const noexcept {
    return tree_.concurrency_model();
  }
  
  /// Set the number of threads for parallel operations
  void set_num_threads(size_t num_threads) {
    num_threads_ = num_threads > 0 ? num_threads : 1;
    tree_.set_num_threads(num_threads_);
  }
  
  /// Get the number of threads for parallel operations
  size_t num_threads() const noexcept {
    return num_threads_;
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

  /// Build tree from multiple sample files in parallel using the specified concurrency model
  /// @param sample_files Vector of (file_path, process_id) pairs
  /// @param time_per_sample Estimated time per sample in microseconds
  /// @return Number of files successfully processed
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth,
            size_t Capacity = 1048576>
  size_t build_from_files_parallel(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample = 1000.0) {
    
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

    ConcurrencyModel model = tree_.concurrency_model();

    if (model == ConcurrencyModel::kSerial) {
      // Serial mode - just call build_from_files
      return build_from_files<MaxDepth, Capacity>(sample_files, time_per_sample);
    }
    
    if (model == ConcurrencyModel::kThreadLocalMerge) {
      return build_from_files_thread_local_merge<MaxDepth, Capacity>(sample_files, time_per_sample);
    }
    
    // For fine-grained and lock-free models, process files in parallel threads
    return build_from_files_concurrent<MaxDepth, Capacity>(sample_files, time_per_sample);
  }

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
  /// Build from files using thread-local merge model
  template <size_t MaxDepth, size_t Capacity>
  size_t build_from_files_thread_local_merge(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample) {
    
    // Create thread-local trees
    std::vector<std::unique_ptr<PerformanceTree>> thread_trees;
    size_t actual_threads = std::min(num_threads_, sample_files.size());
    
    for (size_t t = 0; t < actual_threads; ++t) {
      thread_trees.push_back(tree_.create_thread_local_tree());
    }
    
    // Process files in parallel
    std::atomic<size_t> success_count(0);
    std::vector<std::future<void>> futures;
    
    size_t files_per_thread = (sample_files.size() + actual_threads - 1) / actual_threads;
    
    for (size_t t = 0; t < actual_threads; ++t) {
      size_t start_idx = t * files_per_thread;
      size_t end_idx = std::min(start_idx + files_per_thread, sample_files.size());
      
      if (start_idx >= sample_files.size()) {
        break;
      }
      
      futures.push_back(std::async(std::launch::async, [&, t, start_idx, end_idx]() {
        PerformanceTree& local_tree = *thread_trees[t];
        
        for (size_t i = start_idx; i < end_idx; ++i) {
          const auto& file_pair = sample_files[i];
          
          if (build_from_file_into_tree<MaxDepth, Capacity>(
              file_pair.first.c_str(), file_pair.second, local_tree, time_per_sample)) {
            ++success_count;
          }
        }
      }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
      future.wait();
    }
    
    // Merge all thread-local trees into the main tree
    for (auto& thread_tree : thread_trees) {
      tree_.merge_thread_local_tree(*thread_tree);
    }
    
    return success_count.load();
  }

  /// Build from files using fine-grained or lock-free model
  template <size_t MaxDepth, size_t Capacity>
  size_t build_from_files_concurrent(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      double time_per_sample) {
    
    std::atomic<size_t> success_count(0);
    std::vector<std::future<void>> futures;
    
    size_t actual_threads = std::min(num_threads_, sample_files.size());
    size_t files_per_thread = (sample_files.size() + actual_threads - 1) / actual_threads;
    
    for (size_t t = 0; t < actual_threads; ++t) {
      size_t start_idx = t * files_per_thread;
      size_t end_idx = std::min(start_idx + files_per_thread, sample_files.size());
      
      if (start_idx >= sample_files.size()) {
        break;
      }
      
      futures.push_back(std::async(std::launch::async, [&, start_idx, end_idx]() {
        for (size_t i = start_idx; i < end_idx; ++i) {
          const auto& file_pair = sample_files[i];
          
          if (build_from_file<MaxDepth, Capacity>(
              file_pair.first.c_str(), file_pair.second, time_per_sample)) {
            ++success_count;
          }
        }
      }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
      future.wait();
    }
    
    // For lock-free model, consolidate atomic counters
    if (tree_.concurrency_model() == ConcurrencyModel::kLockFree) {
      tree_.consolidate_atomic_counters();
    }
    
    return success_count.load();
  }

  /// Build from a single file into a specific tree (for thread-local merge)
  template <size_t MaxDepth, size_t Capacity>
  bool build_from_file_into_tree(const char* sample_file, uint32_t process_id,
                                  PerformanceTree& target_tree, double time_per_sample) {
    // Import sample data - allocate on heap to avoid stack overflow
    auto data = std::make_unique<sampling::StaticHashMap<sampling::CallStack<MaxDepth>, uint64_t, Capacity>>();
    sampling::DataImporter importer(sample_file);
    auto result = importer.importData(*data);

    if (result != sampling::DataResult::kSuccess) {
      return false;
    }

    // Convert and insert each call stack
    bool has_converter = converter_.has_snapshot(process_id);

    data->for_each([&target_tree, this, process_id, time_per_sample, has_converter](
                      const sampling::CallStack<MaxDepth>& stack,
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
      target_tree.insert_call_stack(frames, process_id, count, total_time);
    });

    return true;
  }

  OffsetConverter converter_;
  PerformanceTree tree_;
  size_t num_threads_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_TREE_BUILDER_H_
