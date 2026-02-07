// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_TREE_VISUALIZER_H_
#define PERFLOW_ANALYSIS_TREE_VISUALIZER_H_

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "analysis/performance_tree.h"

namespace perflow {
namespace analysis {

/// ColorScheme defines the coloring strategy for visualization
enum class ColorScheme {
  kGrayscale = 0,   // Grayscale based on sample count
  kHeatmap = 1,     // Heat map (blue -> red based on hotness)
  kRainbow = 2,     // Rainbow colors
};

/// TreeVisualizer generates visualizations of performance trees
class TreeVisualizer {
 public:
  /// Generate a DOT file for GraphViz visualization
  /// @param tree Performance tree to visualize
  /// @param output_path Output file path (should end with .dot)
  /// @param scheme Color scheme to use
  /// @param max_depth Maximum depth to visualize (0 = unlimited)
  /// @return True if successful
  static bool generate_dot(const PerformanceTree& tree,
                          const char* output_path,
                          ColorScheme scheme = ColorScheme::kHeatmap,
                          size_t max_depth = 0) {
    FILE* file = std::fopen(output_path, "w");
    if (!file) {
      return false;
    }

    // Write DOT header
    std::fprintf(file, "digraph PerformanceTree {\n");
    std::fprintf(file, "  rankdir=TB;\n");
    std::fprintf(file, "  node [shape=box, style=filled];\n");
    std::fprintf(file, "  edge [arrowhead=vee];\n\n");

    // Find max samples for normalization
    // Use the appropriate sample count based on tree's sample count mode
    uint64_t max_samples = tree.total_samples();
    bool use_self_samples = (tree.sample_count_mode() == SampleCountMode::kExclusive);

    // Generate nodes and edges
    uint64_t node_id = 0;
    generate_dot_node(tree.root(), node_id, max_samples, scheme, file, 0,
                     max_depth, use_self_samples);

    std::fprintf(file, "}\n");
    std::fclose(file);
    return true;
  }

  /// Generate a PDF visualization using GraphViz
  /// @param tree Performance tree to visualize
  /// @param output_pdf Output PDF file path
  /// @param scheme Color scheme to use
  /// @param max_depth Maximum depth to visualize
  /// @return True if successful
  static bool generate_pdf(const PerformanceTree& tree,
                          const char* output_pdf,
                          ColorScheme scheme = ColorScheme::kHeatmap,
                          size_t max_depth = 0) {
    // Generate DOT file first
    char dot_file[4096];
    std::snprintf(dot_file, sizeof(dot_file), "%s.dot", output_pdf);

    if (!generate_dot(tree, dot_file, scheme, max_depth)) {
      return false;
    }

    // Call dot to generate PDF
    char command[8192];
    std::snprintf(command, sizeof(command),
                 "dot -Tpdf \"%s\" -o \"%s\" 2>/dev/null", dot_file,
                 output_pdf);

    int result = std::system(command);

    // Clean up DOT file
    std::remove(dot_file);

    return result == 0;
  }

 private:
  static void generate_dot_node(const std::shared_ptr<TreeNode>& node,
                                uint64_t& node_id, uint64_t max_samples,
                                ColorScheme scheme, FILE* file,
                                size_t current_depth, size_t max_depth,
                                bool use_self_samples) {
    if (!node) return;
    if (max_depth > 0 && current_depth > max_depth) return;

    uint64_t current_id = node_id++;

    // Generate node label
    const auto& frame = node->frame();
    std::string label = frame.function_name;
    
    if (label.empty() || label == "[unknown]") {
      char addr_buf[32];
      std::snprintf(addr_buf, sizeof(addr_buf), "0x%lx",
                   static_cast<unsigned long>(frame.raw_address));
      label = addr_buf;
    }

    // Add sample count - use self_samples in exclusive mode, total_samples otherwise
    uint64_t node_samples = use_self_samples ? node->self_samples() : node->total_samples();
    char info_buf[128];
    std::snprintf(info_buf, sizeof(info_buf), "\\n[%llu samples, %.1f%%]",
                 static_cast<unsigned long long>(node_samples),
                 max_samples > 0 ? (node_samples * 100.0 / max_samples)
                                 : 0.0);
    label += info_buf;

    // Escape special characters in label
    for (size_t i = 0; i < label.length(); ++i) {
      if (label[i] == '"') {
        label.insert(i, 1, '\\');
        ++i;
      }
    }

    // Determine color based on scheme - use appropriate sample count
    std::string color = get_color(node_samples, max_samples, scheme);

    // Write node
    std::fprintf(file, "  node%llu [label=\"%s\", fillcolor=\"%s\"];\n",
                static_cast<unsigned long long>(current_id), label.c_str(),
                color.c_str());

    // Write edges to children
    for (const auto& child : node->children()) {
      uint64_t child_id = node_id;
      std::fprintf(file, "  node%llu -> node%llu",
                  static_cast<unsigned long long>(current_id),
                  static_cast<unsigned long long>(child_id));

      // Add edge label with call count if available
      uint64_t call_count = node->get_call_count(child);
      if (call_count > 0) {
        std::fprintf(file, " [label=\"%llu\"]",
                    static_cast<unsigned long long>(call_count));
      }
      std::fprintf(file, ";\n");

      // Recursively generate child
      generate_dot_node(child, node_id, max_samples, scheme, file,
                       current_depth + 1, max_depth, use_self_samples);
    }
  }

  static std::string get_color(uint64_t samples, uint64_t max_samples,
                               ColorScheme scheme) {
    if (max_samples == 0) {
      return "white";
    }

    double ratio = static_cast<double>(samples) / max_samples;

    switch (scheme) {
      case ColorScheme::kGrayscale: {
        int gray = static_cast<int>((1.0 - ratio) * 255);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", gray, gray, gray);
        return buf;
      }

      case ColorScheme::kHeatmap: {
        // Blue -> Green -> Yellow -> Red
        int r, g, b;
        if (ratio < 0.25) {
          // Blue to Cyan
          r = 0;
          g = static_cast<int>(ratio * 4 * 255);
          b = 255;
        } else if (ratio < 0.5) {
          // Cyan to Green
          r = 0;
          g = 255;
          b = static_cast<int>((0.5 - ratio) * 4 * 255);
        } else if (ratio < 0.75) {
          // Green to Yellow
          r = static_cast<int>((ratio - 0.5) * 4 * 255);
          g = 255;
          b = 0;
        } else {
          // Yellow to Red
          r = 255;
          g = static_cast<int>((1.0 - ratio) * 4 * 255);
          b = 0;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return buf;
      }

      case ColorScheme::kRainbow: {
        // HSV rainbow with H varying from 0 (red) to 270 (purple)
        double h = (1.0 - ratio) * 270.0;
        int r, g, b;
        hsv_to_rgb(h, 1.0, 1.0, r, g, b);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return buf;
      }

      default:
        return "white";
    }
  }

  static void hsv_to_rgb(double h, double s, double v, int& r, int& g,
                        int& b) {
    double c = v * s;
    double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;

    double r1, g1, b1;
    if (h < 60) {
      r1 = c;
      g1 = x;
      b1 = 0;
    } else if (h < 120) {
      r1 = x;
      g1 = c;
      b1 = 0;
    } else if (h < 180) {
      r1 = 0;
      g1 = c;
      b1 = x;
    } else if (h < 240) {
      r1 = 0;
      g1 = x;
      b1 = c;
    } else if (h < 300) {
      r1 = x;
      g1 = 0;
      b1 = c;
    } else {
      r1 = c;
      g1 = 0;
      b1 = x;
    }

    r = static_cast<int>((r1 + m) * 255);
    g = static_cast<int>((g1 + m) * 255);
    b = static_cast<int>((b1 + m) * 255);
  }
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_TREE_VISUALIZER_H_
