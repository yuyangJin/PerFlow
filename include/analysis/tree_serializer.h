// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_TREE_SERIALIZER_H_
#define PERFLOW_ANALYSIS_TREE_SERIALIZER_H_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "analysis/performance_tree.h"

namespace perflow {
namespace analysis {

/// Magic number for tree files
constexpr uint32_t kTreeMagic = 0x50545245;  // "PTRE"

/// Tree file format version
constexpr uint16_t kTreeFormatVersion = 1;

/// Compression types
enum class TreeCompressionType : uint8_t {
  kNone = 0,
  kGzip = 1,
  kZlib = 2,
};

/// Tree file header
struct __attribute__((packed)) TreeFileHeader {
  uint32_t magic;
  uint16_t version;
  uint8_t compression;
  uint8_t reserved1;
  uint64_t node_count;
  uint32_t process_count;
  uint32_t reserved2;
  uint64_t timestamp;
  uint8_t reserved3[32];

  TreeFileHeader() noexcept
      : magic(kTreeMagic),
        version(kTreeFormatVersion),
        compression(static_cast<uint8_t>(TreeCompressionType::kNone)),
        reserved1(0),
        node_count(0),
        process_count(0),
        reserved2(0),
        timestamp(0) {
    std::memset(reserved3, 0, sizeof(reserved3));
  }
};

static_assert(sizeof(TreeFileHeader) == 64, "Header must be 64 bytes");

/// Node entry in serialized format
struct __attribute__((packed)) TreeNodeHeader {
  uint64_t node_id;
  uint64_t parent_id;
  uint64_t total_samples;
  uint64_t self_samples;
  uint64_t raw_address;
  uint64_t offset;
  uint32_t line_number;
  uint16_t function_name_len;
  uint16_t library_name_len;
  uint16_t filename_len;
  uint16_t child_count;
  uint32_t reserved;

  TreeNodeHeader() noexcept
      : node_id(0),
        parent_id(0),
        total_samples(0),
        self_samples(0),
        raw_address(0),
        offset(0),
        line_number(0),
        function_name_len(0),
        library_name_len(0),
        filename_len(0),
        child_count(0),
        reserved(0) {}
};

static_assert(sizeof(TreeNodeHeader) == 64, "Node header must be 64 bytes");

/// TreeSerializer handles serialization of performance trees
class TreeSerializer {
 public:
  /// Export tree to file
  /// @param tree Performance tree to export
  /// @param directory Output directory
  /// @param filename Base filename (without extension)
  /// @param compressed Whether to compress
  /// @return True if successful
  static bool export_tree(const PerformanceTree& tree, const char* directory,
                          const char* filename, bool compressed = false) {
    // Build file path
    char filepath[4096];
    size_t dir_len = std::strlen(directory);
    size_t file_len = std::strlen(filename);

    if (dir_len + file_len + 16 >= sizeof(filepath)) {
      return false;
    }

    std::strcpy(filepath, directory);
    if (dir_len > 0 && filepath[dir_len - 1] != '/') {
      std::strcat(filepath, "/");
    }
    std::strcat(filepath, filename);
    std::strcat(filepath, compressed ? ".ptree.gz" : ".ptree");

    FILE* file = std::fopen(filepath, "wb");
    if (!file) {
      return false;
    }

    // Write header
    TreeFileHeader header;
    header.compression =
        static_cast<uint8_t>(compressed ? TreeCompressionType::kGzip
                                        : TreeCompressionType::kNone);
    header.process_count = static_cast<uint32_t>(tree.process_count());
    header.timestamp = static_cast<uint64_t>(std::time(nullptr));

    // Count nodes
    size_t node_count = 0;
    count_nodes(tree.root(), node_count);
    header.node_count = node_count;

    if (std::fwrite(&header, sizeof(header), 1, file) != 1) {
      std::fclose(file);
      return false;
    }

    // Serialize tree nodes
    uint64_t next_id = 0;
    bool success = serialize_node(tree.root(), 0, next_id, file);

    std::fclose(file);
    return success;
  }

  /// Export tree as human-readable text
  static bool export_tree_text(const PerformanceTree& tree,
                               const char* directory, const char* filename) {
    char filepath[4096];
    size_t dir_len = std::strlen(directory);
    size_t file_len = std::strlen(filename);

    if (dir_len + file_len + 16 >= sizeof(filepath)) {
      return false;
    }

    std::strcpy(filepath, directory);
    if (dir_len > 0 && filepath[dir_len - 1] != '/') {
      std::strcat(filepath, "/");
    }
    std::strcat(filepath, filename);
    std::strcat(filepath, ".ptree.txt");

    FILE* file = std::fopen(filepath, "w");
    if (!file) {
      return false;
    }

    std::fprintf(file, "# PerFlow Performance Tree (Text Format)\n");
    std::fprintf(file, "# Generated: %llu\n",
                 static_cast<unsigned long long>(std::time(nullptr)));
    std::fprintf(file, "# Process count: %zu\n", tree.process_count());
    std::fprintf(file, "# Total samples: %llu\n",
                 static_cast<unsigned long long>(tree.total_samples()));
    std::fprintf(file, "#\n\n");

    print_node_text(tree.root(), 0, file);

    std::fclose(file);
    return true;
  }

 private:
  static void count_nodes(const std::shared_ptr<TreeNode>& node,
                          size_t& count) {
    if (!node) return;
    count++;
    for (const auto& child : node->children()) {
      count_nodes(child, count);
    }
  }

  static bool serialize_node(const std::shared_ptr<TreeNode>& node,
                             uint64_t parent_id, uint64_t& next_id,
                             FILE* file) {
    if (!node) return true;

    uint64_t current_id = next_id++;

    // Prepare node header
    TreeNodeHeader node_header;
    node_header.node_id = current_id;
    node_header.parent_id = parent_id;
    node_header.total_samples = node->total_samples();
    node_header.self_samples = node->self_samples();
    node_header.raw_address = node->frame().raw_address;
    node_header.offset = node->frame().offset;
    node_header.line_number = node->frame().line_number;
    node_header.function_name_len =
        static_cast<uint16_t>(node->frame().function_name.length());
    node_header.library_name_len =
        static_cast<uint16_t>(node->frame().library_name.length());
    node_header.filename_len =
        static_cast<uint16_t>(node->frame().filename.length());
    node_header.child_count = static_cast<uint16_t>(node->children().size());

    // Write node header
    if (std::fwrite(&node_header, sizeof(node_header), 1, file) != 1) {
      return false;
    }

    // Write strings
    if (node_header.function_name_len > 0) {
      if (std::fwrite(node->frame().function_name.c_str(),
                      node_header.function_name_len, 1, file) != 1) {
        return false;
      }
    }
    if (node_header.library_name_len > 0) {
      if (std::fwrite(node->frame().library_name.c_str(),
                      node_header.library_name_len, 1, file) != 1) {
        return false;
      }
    }
    if (node_header.filename_len > 0) {
      if (std::fwrite(node->frame().filename.c_str(), node_header.filename_len,
                      1, file) != 1) {
        return false;
      }
    }

    // Write per-process data
    const auto& counts = node->sampling_counts();
    const auto& times = node->execution_times();

    for (size_t i = 0; i < counts.size(); ++i) {
      uint64_t count = counts[i];
      if (std::fwrite(&count, sizeof(count), 1, file) != 1) {
        return false;
      }
    }

    for (size_t i = 0; i < times.size(); ++i) {
      double time = times[i];
      if (std::fwrite(&time, sizeof(time), 1, file) != 1) {
        return false;
      }
    }

    // Write children
    for (const auto& child : node->children()) {
      if (!serialize_node(child, current_id, next_id, file)) {
        return false;
      }
    }

    return true;
  }

  static void print_node_text(const std::shared_ptr<TreeNode>& node, int depth,
                              FILE* file) {
    if (!node) return;

    // Print indentation
    for (int i = 0; i < depth; ++i) {
      std::fprintf(file, "  ");
    }

    // Print node info
    std::fprintf(file, "[%llu samples, %llu self] %s",
                 static_cast<unsigned long long>(node->total_samples()),
                 static_cast<unsigned long long>(node->self_samples()),
                 node->frame().function_name.c_str());

    if (!node->frame().library_name.empty() &&
        node->frame().library_name != "[virtual]") {
      std::fprintf(file, " (%s)", node->frame().library_name.c_str());
    }

    if (!node->frame().filename.empty() && node->frame().line_number > 0) {
      std::fprintf(file, " [%s:%u]", node->frame().filename.c_str(),
                   node->frame().line_number);
    }

    std::fprintf(file, "\n");

    // Print children
    for (const auto& child : node->children()) {
      print_node_text(child, depth + 1, file);
    }
  }
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_TREE_SERIALIZER_H_
