// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_ONLINE_ANALYSIS_H_
#define PERFLOW_ANALYSIS_ONLINE_ANALYSIS_H_

#include <cctype>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

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
  /// User callback type for file processing notifications
  /// Args: (file_path, file_type, is_new_file)
  using FileProcessCallback =
      std::function<void(const std::string&, FileType, bool)>;

  /// Constructor
  OnlineAnalysis() noexcept : builder_(), monitor_(nullptr), monitor_directory_(),
                               pending_libmaps_(), processed_files_(),
                               file_callback_(), mutex_() {}

  /// Set the directory to monitor
  /// @param directory Directory path
  /// @param poll_interval_ms Polling interval in milliseconds
  void set_monitor_directory(const std::string& directory,
                            uint32_t poll_interval_ms = 1000) {
    monitor_directory_ = directory;
    monitor_ = std::make_unique<DirectoryMonitor>(directory, poll_interval_ms);
    
    // Set up callback to handle new files
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
  
  /// Set user callback for file processing notifications
  void set_file_callback(FileProcessCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_callback_ = callback;
  }
  
  /// Get the monitored directory
  const std::string& monitor_directory() const { return monitor_directory_; }

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
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Track which files we've processed
    if (processed_files_.find(path) != processed_files_.end() && is_new) {
      return;  // Already processed this file
    }
    
    switch (type) {
      case FileType::kLibraryMap: {
        // Extract rank ID from filename (e.g., "perflow_mpi_rank_0.libmap")
        uint32_t rank_id = extract_rank_from_filename(path);
        if (rank_id != UINT32_MAX) {
          pending_libmaps_[rank_id] = path;
          if (file_callback_) {
            file_callback_(path, type, is_new);
          }
        }
        break;
      }
      
      case FileType::kSampleData: {
        // Extract rank ID from filename (e.g., "perflow_mpi_rank_0.pflw")
        uint32_t rank_id = extract_rank_from_filename(path);
        if (rank_id != UINT32_MAX) {
          // Load library map for this rank if available
          auto libmap_it = pending_libmaps_.find(rank_id);
          if (libmap_it != pending_libmaps_.end()) {
            std::vector<std::pair<std::string, uint32_t>> libmaps = {{libmap_it->second, rank_id}};
            builder_.load_library_maps(libmaps);
            pending_libmaps_.erase(libmap_it);
          }
          
          // Build tree from this sample file
          std::vector<std::pair<std::string, uint32_t>> files = {{path, rank_id}};
          builder_.build_from_files(files, 1000.0);
          processed_files_.insert(path);
          
          if (file_callback_) {
            file_callback_(path, type, is_new);
          }
        }
        break;
      }
      
      case FileType::kPerformanceTree:
        // Could be used for loading pre-built trees in the future
        if (file_callback_) {
          file_callback_(path, type, is_new);
        }
        break;
        
      default:
        break;
    }
  }
  
  /// Extract rank ID from filename like "perflow_mpi_rank_0.pflw"
  static uint32_t extract_rank_from_filename(const std::string& path) {
    // Find "rank_" in the filename
    size_t pos = path.find("rank_");
    if (pos == std::string::npos) {
      return UINT32_MAX;  // Not found
    }
    
    pos += 5;  // Move past "rank_"
    
    // Extract digits
    size_t end_pos = pos;
    while (end_pos < path.length() && std::isdigit(path[end_pos])) {
      ++end_pos;
    }
    
    if (end_pos > pos) {
      try {
        return static_cast<uint32_t>(std::stoul(path.substr(pos, end_pos - pos)));
      } catch (...) {
        return UINT32_MAX;
      }
    }
    
    return UINT32_MAX;
  }

  TreeBuilder builder_;
  std::unique_ptr<DirectoryMonitor> monitor_;
  std::string monitor_directory_;
  std::unordered_map<uint32_t, std::string> pending_libmaps_;
  std::unordered_set<std::string> processed_files_;
  FileProcessCallback file_callback_;
  mutable std::mutex mutex_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_ONLINE_ANALYSIS_H_
