// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_ANALYSIS_TASKS_H_
#define PERFLOW_ANALYSIS_ANALYSIS_TASKS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "analysis/performance_tree.h"

namespace perflow {
namespace analysis {

/// BalanceAnalysisResult contains workload balance information
struct BalanceAnalysisResult {
  double mean_samples;
  double std_dev_samples;
  double min_samples;
  double max_samples;
  double imbalance_factor;  // (max - min) / mean
  size_t most_loaded_process;
  size_t least_loaded_process;
  std::vector<double> process_samples;

  BalanceAnalysisResult()
      : mean_samples(0),
        std_dev_samples(0),
        min_samples(0),
        max_samples(0),
        imbalance_factor(0),
        most_loaded_process(0),
        least_loaded_process(0),
        process_samples() {}
};

/// HotspotInfo describes a performance hotspot
struct HotspotInfo {
  std::string function_name;
  std::string library_name;
  std::string source_location;
  uint64_t total_samples;
  double percentage;
  uint64_t self_samples;
  double self_percentage;

  HotspotInfo()
      : function_name(),
        library_name(),
        source_location(),
        total_samples(0),
        percentage(0),
        self_samples(0),
        self_percentage(0) {}
};

/// BalanceAnalyzer analyzes workload distribution across processes
class BalanceAnalyzer {
 public:
  /// Analyze workload balance in the performance tree
  /// @param tree Performance tree to analyze
  /// @return Balance analysis result
  static BalanceAnalysisResult analyze(const PerformanceTree& tree) {
    BalanceAnalysisResult result;

    size_t process_count = tree.process_count();
    if (process_count == 0) {
      return result;
    }

    // Collect per-process sample counts from root
    auto root = tree.root();
    if (!root) {
      return result;
    }

    const auto& counts = root->sampling_counts();
    result.process_samples.resize(process_count);

    // If counts is smaller than process_count, some processes have 0 samples
    size_t counts_size = counts.size();

    // Calculate statistics
    double sum = 0;
    result.min_samples = std::numeric_limits<double>::max();
    result.max_samples = 0;

    for (size_t i = 0; i < process_count; ++i) {
      double count = (i < counts_size) ? static_cast<double>(counts[i]) : 0.0;
      result.process_samples[i] = count;
      sum += count;

      if (count < result.min_samples) {
        result.min_samples = count;
        result.least_loaded_process = i;
      }
      if (count > result.max_samples) {
        result.max_samples = count;
        result.most_loaded_process = i;
      }
    }

    result.mean_samples = sum / process_count;

    // Calculate standard deviation
    double variance_sum = 0;
    for (size_t i = 0; i < process_count; ++i) {
      double diff = result.process_samples[i] - result.mean_samples;
      variance_sum += diff * diff;
    }
    result.std_dev_samples = std::sqrt(variance_sum / process_count);

    // Calculate imbalance factor
    if (result.mean_samples > 0) {
      result.imbalance_factor =
          (result.max_samples - result.min_samples) / result.mean_samples;
    }

    return result;
  }
};

/// HotspotAnalyzer identifies performance bottlenecks
class HotspotAnalyzer {
 public:
  /// Find top hotspots in the performance tree
  /// @param tree Performance tree to analyze
  /// @param top_n Number of top hotspots to return
  /// @return Vector of hotspot information
  static std::vector<HotspotInfo> find_hotspots(const PerformanceTree& tree,
                                                 size_t top_n = 10) {
    std::vector<HotspotInfo> hotspots;
    uint64_t total_samples = tree.total_samples();

    if (total_samples == 0) {
      return hotspots;
    }

    // Collect all nodes
    std::vector<std::shared_ptr<TreeNode>> nodes;
    collect_nodes(tree.root(), nodes);

    // Sort by total samples (descending)
    std::sort(nodes.begin(), nodes.end(),
             [](const std::shared_ptr<TreeNode>& a,
                const std::shared_ptr<TreeNode>& b) {
               return a->total_samples() > b->total_samples();
             });

    // Extract top N hotspots
    size_t count = std::min(top_n, nodes.size());
    for (size_t i = 0; i < count; ++i) {
      const auto& node = nodes[i];
      const auto& frame = node->frame();

      HotspotInfo info;
      info.function_name = frame.function_name;
      info.library_name = frame.library_name;

      if (!frame.filename.empty() && frame.line_number > 0) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s:%u", frame.filename.c_str(),
                     frame.line_number);
        info.source_location = buf;
      }

      info.total_samples = node->total_samples();
      info.percentage = (node->total_samples() * 100.0) / total_samples;
      info.self_samples = node->self_samples();
      info.self_percentage = (node->self_samples() * 100.0) / total_samples;

      hotspots.push_back(info);
    }

    return hotspots;
  }

  /// Find hotspots by self samples (exclusive time)
  static std::vector<HotspotInfo> find_self_hotspots(
      const PerformanceTree& tree, size_t top_n = 10) {
    std::vector<HotspotInfo> hotspots;
    uint64_t total_samples = tree.total_samples();

    if (total_samples == 0) {
      return hotspots;
    }

    // Collect all nodes
    std::vector<std::shared_ptr<TreeNode>> nodes;
    collect_nodes(tree.root(), nodes);

    // Sort by self samples (descending)
    std::sort(nodes.begin(), nodes.end(),
             [](const std::shared_ptr<TreeNode>& a,
                const std::shared_ptr<TreeNode>& b) {
               return a->self_samples() > b->self_samples();
             });

    // Extract top N hotspots
    size_t count = std::min(top_n, nodes.size());
    for (size_t i = 0; i < count; ++i) {
      const auto& node = nodes[i];

      // Skip nodes with no self samples
      if (node->self_samples() == 0) {
        continue;
      }

      const auto& frame = node->frame();

      HotspotInfo info;
      info.function_name = frame.function_name;
      info.library_name = frame.library_name;

      if (!frame.filename.empty() && frame.line_number > 0) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s:%u", frame.filename.c_str(),
                     frame.line_number);
        info.source_location = buf;
      }

      info.total_samples = node->total_samples();
      info.percentage = (node->total_samples() * 100.0) / total_samples;
      info.self_samples = node->self_samples();
      info.self_percentage = (node->self_samples() * 100.0) / total_samples;

      hotspots.push_back(info);
    }

    return hotspots;
  }

 private:
  static void collect_nodes(const std::shared_ptr<TreeNode>& node,
                            std::vector<std::shared_ptr<TreeNode>>& nodes) {
    if (!node) return;

    // Skip virtual root
    if (node->frame().function_name != "[root]") {
      nodes.push_back(node);
    }

    for (const auto& child : node->children()) {
      collect_nodes(child, nodes);
    }
  }
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_ANALYSIS_TASKS_H_
