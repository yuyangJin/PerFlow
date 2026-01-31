// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

// Example: Online monitoring and analysis of MPI application performance data
// This example demonstrates continuous directory monitoring where:
// 1. The analyzer starts first and monitors a directory
// 2. An MPI application with PerFlow sampler outputs data to the monitored directory
// 3. The analyzer detects new files and analyzes them incrementally
// 4. Monitoring continues indefinitely until interrupted

#include <csignal>
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

#include "analysis/online_analysis.h"

using namespace perflow::analysis;

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    std::cout << "\n\nReceived shutdown signal. Stopping monitoring...\n";
    g_running.store(false);
  }
}

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " <data_directory> [output_directory]\n\n";
  std::cout << "Arguments:\n";
  std::cout << "  data_directory     Directory to monitor for .pflw and .libmap files\n";
  std::cout << "  output_directory   Directory for output files (default: /tmp)\n\n";
  std::cout << "Example:\n";
  std::cout << "  " << program_name << " /tmp/perflow_samples /tmp/output\n\n";
  std::cout << "Workflow:\n";
  std::cout << "  1. Start this analyzer to begin monitoring\n";
  std::cout << "  2. In another terminal, run your MPI application:\n";
  std::cout << "       LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \\\n";
  std::cout << "       PERFLOW_OUTPUT_DIR=/tmp/perflow_samples \\\n";
  std::cout << "       mpirun -n 4 ./your_mpi_app\n";
  std::cout << "  3. Watch as the analyzer processes new files in real-time\n";
  std::cout << "  4. Press Ctrl+C to stop monitoring and generate final reports\n\n";
}

void print_analysis_summary(const OnlineAnalysis& analysis, const char* output_dir) {
  const PerformanceTree& tree = analysis.tree();
  
  std::cout << "\n========================================\n";
  std::cout << "Current Analysis Summary\n";
  std::cout << "========================================\n\n";
  
  if (tree.total_samples() == 0) {
    std::cout << "No samples collected yet. Waiting for data...\n";
    return;
  }
  
  std::cout << "Performance Tree Statistics:\n";
  std::cout << "  Total samples: " << tree.total_samples() << "\n";
  std::cout << "  Process count: " << tree.process_count() << "\n";
  std::cout << "  Root children: " << tree.root()->children().size() << "\n\n";
  
  // Balance analysis
  auto balance = analysis.analyze_balance();
  std::cout << "Balance Analysis:\n";
  std::cout << "  Mean samples per process: " << balance.mean_samples << "\n";
  std::cout << "  Std deviation: " << balance.std_dev_samples << "\n";
  std::cout << "  Min samples: " << balance.min_samples
            << " (process " << balance.least_loaded_process << ")\n";
  std::cout << "  Max samples: " << balance.max_samples
            << " (process " << balance.most_loaded_process << ")\n";
  std::cout << "  Imbalance factor: " << balance.imbalance_factor << "\n\n";
  
  // Top 5 hotspots
  std::cout << "Top 5 Hotspots (Total Time):\n";
  auto hotspots = analysis.find_hotspots(5);
  for (size_t i = 0; i < hotspots.size(); ++i) {
    const auto& hotspot = hotspots[i];
    std::cout << "  " << (i + 1) << ". " << hotspot.function_name;
    if (!hotspot.library_name.empty()) {
      std::cout << " (" << hotspot.library_name << ")";
    }
    std::cout << " - " << hotspot.total_samples << " samples ("
              << hotspot.percentage << "%)\n";
  }
  
  // Export visualization
  std::string viz_path = std::string(output_dir) + "/performance_tree_live.pdf";
  if (analysis.export_visualization(viz_path.c_str(), ColorScheme::kHeatmap, 15)) {
    std::cout << "\n  Visualization saved: " << viz_path << "\n";
  }
  
  std::cout << "========================================\n\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char* data_dir = argv[1];
  const char* output_dir = (argc > 2) ? argv[2] : "/tmp";

  // Set up signal handlers for graceful shutdown
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "PerFlow Online Analysis - Continuous Monitoring Mode\n";
  std::cout << "====================================================\n\n";
  std::cout << "Monitored directory: " << data_dir << "\n";
  std::cout << "Output directory: " << output_dir << "\n";
  std::cout << "Press Ctrl+C to stop monitoring\n\n";

  // Create online analysis instance
  OnlineAnalysis analysis;

  // Set up file processing callback to report activity
  std::atomic<size_t> libmaps_loaded(0);
  std::atomic<size_t> samples_loaded(0);
  
  analysis.set_file_callback([&](const std::string& path, FileType type, bool is_new) {
    (void)is_new;  // Unused but required by callback signature
    if (type == FileType::kLibraryMap) {
      libmaps_loaded++;
      std::cout << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
                << "] Loaded library map: " << path << "\n";
    } else if (type == FileType::kSampleData) {
      samples_loaded++;
      std::cout << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
                << "] Processed sample file: " << path << "\n";
      std::cout << "  Total samples so far: " << analysis.tree().total_samples() << "\n";
    }
  });

  // Start monitoring
  analysis.set_monitor_directory(data_dir, 1000);  // Poll every 1 second
  
  if (!analysis.start_monitoring()) {
    std::cerr << "Failed to start directory monitoring\n";
    return 1;
  }

  std::cout << "Monitoring started. Waiting for sample files...\n\n";
  std::cout << "Tip: Run your MPI application now with:\n";
  std::cout << "  LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \\\n";
  std::cout << "  PERFLOW_OUTPUT_DIR=" << data_dir << " \\\n";
  std::cout << "  mpirun -n <N> ./your_mpi_app\n\n";

  // Main monitoring loop - print periodic updates
  size_t last_sample_count = 0;
  auto last_summary_time = std::chrono::steady_clock::now();
  const auto summary_interval = std::chrono::seconds(10);
  
  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Print periodic summary every 10 seconds if there's data
    auto now = std::chrono::steady_clock::now();
    if (now - last_summary_time >= summary_interval) {
      size_t current_samples = analysis.tree().total_samples();
      if (current_samples > 0 && current_samples != last_sample_count) {
        print_analysis_summary(analysis, output_dir);
        last_sample_count = current_samples;
      }
      last_summary_time = now;
    }
  }

  // Graceful shutdown
  analysis.stop_monitoring();
  
  std::cout << "\nMonitoring stopped.\n\n";
  
  // Final report
  std::cout << "========================================\n";
  std::cout << "Final Analysis Report\n";
  std::cout << "========================================\n\n";
  
  std::cout << "Files processed:\n";
  std::cout << "  Library maps: " << libmaps_loaded.load() << "\n";
  std::cout << "  Sample files: " << samples_loaded.load() << "\n\n";
  
  const PerformanceTree& tree = analysis.tree();
  
  if (tree.total_samples() == 0) {
    std::cout << "No samples were collected during this session.\n";
    std::cout << "Make sure to run your MPI application with the PerFlow sampler.\n";
    return 0;
  }
  
  std::cout << "Performance Tree Statistics:\n";
  std::cout << "  Total samples: " << tree.total_samples() << "\n";
  std::cout << "  Process count: " << tree.process_count() << "\n\n";
  
  // Balance analysis
  std::cout << "Balance Analysis:\n";
  std::cout << "----------------\n";
  auto balance = analysis.analyze_balance();
  std::cout << "  Mean samples per process: " << balance.mean_samples << "\n";
  std::cout << "  Std deviation: " << balance.std_dev_samples << "\n";
  std::cout << "  Min samples: " << balance.min_samples
            << " (process " << balance.least_loaded_process << ")\n";
  std::cout << "  Max samples: " << balance.max_samples
            << " (process " << balance.most_loaded_process << ")\n";
  std::cout << "  Imbalance factor: " << balance.imbalance_factor << "\n\n";
  
  // Top 10 hotspots
  std::cout << "Top 10 Hotspots (Total Time):\n";
  std::cout << "----------------------------\n";
  auto hotspots = analysis.find_hotspots(10);
  for (size_t i = 0; i < hotspots.size(); ++i) {
    const auto& hotspot = hotspots[i];
    std::cout << "  " << (i + 1) << ". " << hotspot.function_name;
    if (!hotspot.library_name.empty()) {
      std::cout << " (" << hotspot.library_name << ")";
    }
    std::cout << "\n";
    std::cout << "     Total: " << hotspot.total_samples << " samples ("
              << hotspot.percentage << "%)\n";
    std::cout << "     Self:  " << hotspot.self_samples << " samples ("
              << hotspot.self_percentage << "%)\n";
  }
  std::cout << "\n";
  
  // Top 10 self-time hotspots
  std::cout << "Top 10 Hotspots (Self Time):\n";
  std::cout << "---------------------------\n";
  auto self_hotspots = analysis.find_self_hotspots(10);
  for (size_t i = 0; i < self_hotspots.size(); ++i) {
    const auto& hotspot = self_hotspots[i];
    std::cout << "  " << (i + 1) << ". " << hotspot.function_name;
    if (!hotspot.library_name.empty()) {
      std::cout << " (" << hotspot.library_name << ")";
    }
    std::cout << " - " << hotspot.self_samples << " samples ("
              << hotspot.self_percentage << "%)\n";
  }
  std::cout << "\n";
  
  // Export results
  std::cout << "Exporting results...\n";
  
  if (analysis.export_tree_text(output_dir, "performance_tree_final")) {
    std::cout << "  Tree (text): " << output_dir << "/performance_tree_final.ptree.txt\n";
  }
  
  if (analysis.export_tree(output_dir, "performance_tree_final", true)) {
    std::cout << "  Tree (compressed): " << output_dir << "/performance_tree_final.ptree\n";
  }
  
  std::string final_viz = std::string(output_dir) + "/performance_tree_final.pdf";
  if (analysis.export_visualization(final_viz.c_str(), ColorScheme::kHeatmap, 20)) {
    std::cout << "  Visualization: " << final_viz << "\n";
  } else {
    std::cout << "  Visualization: skipped (GraphViz not available)\n";
  }
  
  std::cout << "\nAnalysis complete!\n";

  return 0;
}
