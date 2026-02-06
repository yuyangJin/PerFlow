// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_PARALLEL_FILE_READER_H_
#define PERFLOW_ANALYSIS_PARALLEL_FILE_READER_H_

#include <atomic>
#include <cstdint>
#include <future>
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

/// ParallelFileReader provides parallel file reading capabilities for sample data
/// Files are processed in parallel threads and results are merged into a single tree
class ParallelFileReader {
 public:
  /// Status of a file read operation
  struct FileReadResult {
    std::string filepath;
    uint32_t process_id;
    bool success;
    size_t samples_read;
    std::string error_message;

    FileReadResult() 
        : filepath(), process_id(0), success(false), 
          samples_read(0), error_message() {}
  };

  /// Constructor with optional thread count
  /// @param num_threads Number of threads to use (0 = auto-detect)
  explicit ParallelFileReader(size_t num_threads = 0) noexcept
      : num_threads_(num_threads == 0 ? std::thread::hardware_concurrency() : num_threads),
        progress_callback_() {
    if (num_threads_ == 0) {
      num_threads_ = 1;  // Fallback if hardware_concurrency returns 0
    }
  }

  /// Set progress callback
  /// @param callback Function called with (files_completed, total_files)
  void set_progress_callback(std::function<void(size_t, size_t)> callback) {
    progress_callback_ = callback;
  }

  /// Read multiple sample files in parallel and build a performance tree
  /// @tparam MaxDepth Maximum stack depth
  /// @tparam Capacity Hash map capacity
  /// @param sample_files Vector of (file_path, process_id) pairs
  /// @param tree Performance tree to populate
  /// @param converter Offset converter for address resolution
  /// @param time_per_sample Estimated time per sample in microseconds
  /// @return Vector of file read results
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth,
            size_t Capacity = 1048576>
  std::vector<FileReadResult> read_files_parallel(
      const std::vector<std::pair<std::string, uint32_t>>& sample_files,
      PerformanceTree& tree,
      OffsetConverter& converter,
      double time_per_sample = 1000.0) {
    
    std::vector<FileReadResult> results(sample_files.size());
    
    if (sample_files.empty()) {
      return results;
    }

    // Determine max process ID for tree initialization
    size_t max_process_id = 0;
    for (const auto& pair : sample_files) {
      if (pair.second > max_process_id) {
        max_process_id = pair.second;
      }
    }
    tree.set_process_count(max_process_id + 1);

    // Progress tracking
    std::atomic<size_t> completed_files(0);
    size_t total_files = sample_files.size();

    // Create per-file trees to avoid contention
    std::vector<std::unique_ptr<PerformanceTree>> per_file_trees;
    per_file_trees.reserve(sample_files.size());
    for (size_t i = 0; i < sample_files.size(); ++i) {
      per_file_trees.push_back(std::make_unique<PerformanceTree>(
          tree.build_mode(), tree.sample_count_mode()));
      per_file_trees[i]->set_process_count(max_process_id + 1);
    }

    // Process files in parallel using thread pool
    std::vector<std::future<void>> futures;
    size_t files_per_thread = (sample_files.size() + num_threads_ - 1) / num_threads_;
    
    for (size_t t = 0; t < num_threads_; ++t) {
      size_t start_idx = t * files_per_thread;
      size_t end_idx = std::min(start_idx + files_per_thread, sample_files.size());
      
      if (start_idx >= sample_files.size()) {
        break;
      }
      
      futures.push_back(std::async(std::launch::async, [&, start_idx, end_idx]() {
        for (size_t i = start_idx; i < end_idx; ++i) {
          const auto& file_pair = sample_files[i];
          results[i] = read_single_file<MaxDepth, Capacity>(
              file_pair.first, file_pair.second,
              *per_file_trees[i], converter, time_per_sample);
          
          size_t current = ++completed_files;
          if (progress_callback_) {
            progress_callback_(current, total_files);
          }
        }
      }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
      future.wait();
    }

    // Merge per-file trees into main tree
    for (size_t i = 0; i < sample_files.size(); ++i) {
      if (results[i].success) {
        merge_tree(*per_file_trees[i], tree);
      }
    }

    return results;
  }

  /// Get the number of threads used
  size_t thread_count() const noexcept { return num_threads_; }

 private:
  /// Read a single sample file
  template <size_t MaxDepth, size_t Capacity>
  FileReadResult read_single_file(
      const std::string& filepath,
      uint32_t process_id,
      PerformanceTree& tree,
      OffsetConverter& converter,
      double time_per_sample) {
    
    FileReadResult result;
    result.filepath = filepath;
    result.process_id = process_id;

    // Import sample data
    auto data = std::make_unique<sampling::StaticHashMap<
        sampling::CallStack<MaxDepth>, uint64_t, Capacity>>();
    sampling::DataImporter importer(filepath.c_str());
    auto import_result = importer.importData(*data);

    if (import_result != sampling::DataResult::kSuccess) {
      result.success = false;
      result.error_message = "Failed to import data file";
      return result;
    }

    // Convert and insert each call stack
    bool has_converter = converter.has_snapshot(process_id);
    size_t samples_read = 0;

    data->for_each([&tree, &converter, process_id, time_per_sample, 
                    has_converter, &samples_read](
                       const sampling::CallStack<MaxDepth>& stack,
                       const uint64_t& count) {
      std::vector<sampling::ResolvedFrame> frames;

      if (has_converter) {
        bool resolve_symbols = converter.has_symbol_resolver();
        frames = converter.convert(stack, process_id, resolve_symbols);
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
      tree.insert_call_stack(frames, process_id, count, total_time);
      samples_read += count;
    });

    result.success = true;
    result.samples_read = samples_read;
    return result;
  }

  /// Merge source tree into destination tree
  void merge_tree(const PerformanceTree& source, PerformanceTree& dest) {
    // Get all nodes from source and re-insert their call stacks
    // This is a simplified merge - for full merge we'd need to
    // reconstruct call stacks from the tree structure
    merge_node_recursive(source.root(), dest, std::vector<sampling::ResolvedFrame>());
  }

  void merge_node_recursive(
      const std::shared_ptr<TreeNode>& node,
      PerformanceTree& dest,
      std::vector<sampling::ResolvedFrame> current_path) {
    
    if (!node) return;

    // Skip root node
    if (node->frame().function_name != "[root]") {
      current_path.push_back(node->frame());
      
      // If this is a leaf, insert the call stack with its samples
      if (node->is_leaf()) {
        const auto& counts = node->sampling_counts();
        const auto& times = node->execution_times();
        
        for (size_t pid = 0; pid < counts.size(); ++pid) {
          if (counts[pid] > 0) {
            dest.insert_call_stack(current_path, pid, counts[pid], 
                times.size() > pid ? times[pid] : 0.0);
          }
        }
      }
    }

    // Recurse to children
    for (const auto& child : node->children()) {
      merge_node_recursive(child, dest, current_path);
    }
  }

  size_t num_threads_;
  std::function<void(size_t, size_t)> progress_callback_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_PARALLEL_FILE_READER_H_
