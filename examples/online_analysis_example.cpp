// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

// Example: Building and analyzing a performance tree from sample data

#include <iostream>

#include "analysis/online_analysis.h"

using namespace perflow::analysis;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <data_directory>" << std::endl;
    std::cerr << "Example: " << argv[0] << " /tmp/perflow_samples" << std::endl;
    return 1;
  }

  const char* data_dir = argv[1];
  const char* output_dir = (argc > 2) ? argv[2] : "/tmp";

  std::cout << "PerFlow Online Analysis Example\n";
  std::cout << "================================\n\n";

  // Create online analysis instance
  OnlineAnalysis analysis;

  // Load sample files manually (not using monitoring)
  std::cout << "Building performance tree from sample data...\n";

  TreeBuilder& builder = analysis.builder();

  // Example: Load sample files manually
  // In a real scenario, you would scan the directory for .pflw files
  std::vector<std::pair<std::string, uint32_t>> sample_files;
  
  // For this example, we'll just demonstrate the API
  // Users would populate this with actual files from the directory
  
  std::cout << "  Sample files loaded: " << sample_files.size() << "\n";

  if (sample_files.empty()) {
    std::cout << "\nNo sample files found in " << data_dir << "\n";
    std::cout << "This is an example program. In real usage:\n";
    std::cout << "1. Run MPI applications with PerFlow sampler\n";
    std::cout << "2. Collect .pflw sample files\n";
    std::cout << "3. Run this tool to analyze the data\n";
    return 0;
  }

  // Build tree from files
  size_t loaded = builder.build_from_files(sample_files, 1000.0);
  std::cout << "  Successfully loaded: " << loaded << " files\n";

  const PerformanceTree& tree = analysis.tree();
  std::cout << "  Total samples: " << tree.total_samples() << "\n";
  std::cout << "  Process count: " << tree.process_count() << "\n\n";

  // Perform balance analysis
  std::cout << "Balance Analysis\n";
  std::cout << "----------------\n";
  auto balance = analysis.analyze_balance();
  std::cout << "  Mean samples per process: " << balance.mean_samples << "\n";
  std::cout << "  Std deviation: " << balance.std_dev_samples << "\n";
  std::cout << "  Min samples: " << balance.min_samples
            << " (process " << balance.least_loaded_process << ")\n";
  std::cout << "  Max samples: " << balance.max_samples
            << " (process " << balance.most_loaded_process << ")\n";
  std::cout << "  Imbalance factor: " << balance.imbalance_factor << "\n\n";

  // Find hotspots
  std::cout << "Top 10 Hotspots (Total Time)\n";
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
    if (!hotspot.source_location.empty()) {
      std::cout << "     Location: " << hotspot.source_location << "\n";
    }
  }
  std::cout << "\n";

  // Find self-time hotspots
  std::cout << "Top 10 Hotspots (Self Time)\n";
  std::cout << "---------------------------\n";
  auto self_hotspots = analysis.find_self_hotspots(10);
  for (size_t i = 0; i < self_hotspots.size(); ++i) {
    const auto& hotspot = self_hotspots[i];
    std::cout << "  " << (i + 1) << ". " << hotspot.function_name;
    if (!hotspot.library_name.empty()) {
      std::cout << " (" << hotspot.library_name << ")";
    }
    std::cout << "\n";
    std::cout << "     Self: " << hotspot.self_samples << " samples ("
              << hotspot.self_percentage << "%)\n";
  }
  std::cout << "\n";

  // Export tree
  std::cout << "Exporting results...\n";
  
  if (analysis.export_tree_text(output_dir, "performance_tree")) {
    std::cout << "  Tree (text): " << output_dir << "/performance_tree.ptree.txt\n";
  }

  if (analysis.export_tree(output_dir, "performance_tree", false)) {
    std::cout << "  Tree (binary): " << output_dir << "/performance_tree.ptree\n";
  }

  // Try to generate visualization (requires GraphViz)
  if (analysis.export_visualization("/tmp/performance_tree.pdf",
                                    ColorScheme::kHeatmap, 10)) {
    std::cout << "  Visualization: /tmp/performance_tree.pdf\n";
  } else {
    std::cout << "  Visualization: skipped (GraphViz not available)\n";
  }

  std::cout << "\nAnalysis complete!\n";

  return 0;
}
