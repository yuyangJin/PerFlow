// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_ONLINE_ANALYSIS_H_
#define PERFLOW_ANALYSIS_ONLINE_ANALYSIS_H_

#include "analysis/analyzers.h"
#include "analysis/directory_monitor.h"
#include "analysis/performance_tree.h"
#include "analysis/tree_builder.h"
#include "analysis/tree_serializer.h"
#include "analysis/tree_visualizer.h"

namespace perflow {
namespace analysis {

/// OnlineAnalysis provides a complete online analysis framework
/// It monitors directories, builds performance trees, and performs analysis
class OnlineAnalysis {
 public:
  /// Constructor
  OnlineAnalysis() noexcept : builder_(), monitor_(nullptr) {}

  /// Set the directory to monitor
  /// @param directory Directory path
  /// @param poll_interval_ms Polling interval in milliseconds
  void set_monitor_directory(const std::string& directory,
                            uint32_t poll_interval_ms = 1000) {
    monitor_ = std::make_unique<DirectoryMonitor>(directory, poll_interval_ms);
    
    // Set up callback to handle new files
    // TODO(Issue #TBD): Implement file handling logic for automatic tree building
    // Should process new .pflw files and update tree incrementally
    monitor_->set_callback([this](const std::string& path, FileType type,
                                  bool is_new) { handle_file(path, type, is_new); });
  }

  /// Start monitoring
  bool start_monitoring() {
    if (monitor_) {
      return monitor_->start();
    }
    return false;
  }

  /// Stop monitoring
  void stop_monitoring() {
    if (monitor_) {
      monitor_->stop();
    }
  }

  /// Get the tree builder
  TreeBuilder& builder() noexcept { return builder_; }
  const TreeBuilder& builder() const noexcept { return builder_; }

  /// Get the performance tree
  PerformanceTree& tree() noexcept { return builder_.tree(); }
  const PerformanceTree& tree() const noexcept { return builder_.tree(); }

  /// Perform balance analysis
  BalanceAnalysisResult analyze_balance() const {
    return BalanceAnalyzer::analyze(tree());
  }

  /// Find performance hotspots
  std::vector<HotspotInfo> find_hotspots(size_t top_n = 10) const {
    return HotspotAnalyzer::find_hotspots(tree(), top_n);
  }

  /// Find self-time hotspots
  std::vector<HotspotInfo> find_self_hotspots(size_t top_n = 10) const {
    return HotspotAnalyzer::find_self_hotspots(tree(), top_n);
  }

  /// Export tree visualization
  bool export_visualization(const char* output_pdf,
                           ColorScheme scheme = ColorScheme::kHeatmap,
                           size_t max_depth = 0) const {
    return TreeVisualizer::generate_pdf(tree(), output_pdf, scheme, max_depth);
  }

  /// Export tree data
  bool export_tree(const char* directory, const char* filename,
                  bool compressed = false) const {
    return TreeSerializer::export_tree(tree(), directory, filename, compressed);
  }

  /// Export tree as text
  bool export_tree_text(const char* directory, const char* filename) const {
    return TreeSerializer::export_tree_text(tree(), directory, filename);
  }

 private:
  void handle_file(const std::string& path, FileType type, bool is_new) {
    // Handle different file types
    // TODO(Issue #TBD): Implement automatic file processing
    // - For .pflw files: Extract process ID from filename and call builder.build_from_file()
    // - For .libmap files: Load library map with builder.load_library_maps()
    // - For .ptree files: Could be used for loading pre-built trees
    // Returns: void currently, but could return bool for success/failure status
    (void)path;
    (void)type;
    (void)is_new;
  }

  TreeBuilder builder_;
  std::unique_ptr<DirectoryMonitor> monitor_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_ONLINE_ANALYSIS_H_
