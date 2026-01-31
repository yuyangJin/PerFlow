// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_TREE_BUILDER_H_
#define PERFLOW_ANALYSIS_TREE_BUILDER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "analysis/offset_converter.h"
#include "analysis/performance_tree.h"
#include "sampling/call_stack.h"
#include "sampling/data_export.h"

namespace perflow {
namespace analysis {

/// TreeBuilder constructs performance trees from sample data
class TreeBuilder {
 public:
  /// Constructor with optional build mode
  explicit TreeBuilder(TreeBuildMode mode = TreeBuildMode::kContextFree) noexcept 
      : converter_(), tree_(mode) {}

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
        // Convert using offset converter
        frames = converter_ref.convert(stack, process_id);
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
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_TREE_BUILDER_H_
