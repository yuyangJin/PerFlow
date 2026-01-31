// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file post_analysis_example.cpp
/// @brief Example demonstrating post-analysis conversion of raw addresses to offsets
///
/// This example shows how to:
/// 1. Import library maps from files
/// 2. Import call stack data from sampling files
/// 3. Use OffsetConverter to convert raw addresses to resolved frames
/// 4. Process and analyze the resolved frames
///
/// Usage:
///   // Assuming you have collected data with the MPI sampler
///   // The sampler will generate files like:
///   //   - perflow_mpi_rank_0.pflw (call stack data)
///   //   - perflow_mpi_rank_0.libmap (library map)
///   //   - perflow_mpi_rank_0.txt (human-readable data)
///
///   // This example shows how to process them in post-analysis

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/library_map.h"
#include "sampling/static_hash_map.h"
#include "analysis/offset_converter.h"

using namespace perflow::sampling;
using namespace perflow::analysis;

/// Example: Load and convert call stack data for a single rank
void process_rank_data(const char* data_dir, uint32_t rank) {
    // Build filenames
    char data_file[256];
    char libmap_file[256];
    std::snprintf(data_file, sizeof(data_file), 
                  "%s/perflow_mpi_rank_%u.pflw", data_dir, rank);
    std::snprintf(libmap_file, sizeof(libmap_file),
                  "%s/perflow_mpi_rank_%u.libmap", data_dir, rank);
    
    std::cout << "Processing data for rank " << rank << "\n";
    std::cout << "  Data file: " << data_file << "\n";
    std::cout << "  LibMap file: " << libmap_file << "\n";
    
    // 1. Load library map
    LibraryMapImporter map_importer(libmap_file);
    LibraryMap lib_map;
    uint32_t process_id = 0;
    
    DataResult map_result = map_importer.importMap(lib_map, &process_id);
    if (map_result != DataResult::kSuccess) {
        std::cerr << "Failed to import library map for rank " << rank << "\n";
        std::cerr << "  File: " << libmap_file << "\n";
        std::cerr << "  Error code: " << static_cast<int>(map_result) << "\n";
        
        // Provide helpful error messages based on error code
        switch (map_result) {
            case DataResult::kErrorFileOpen:
                std::cerr << "  Reason: File not found or cannot be opened\n";
                std::cerr << "  Hint: Make sure you have run the MPI sampler first to generate .libmap files\n";
                break;
            case DataResult::kErrorFileRead:
                std::cerr << "  Reason: Error reading file\n";
                break;
            case DataResult::kErrorInvalidFormat:
                std::cerr << "  Reason: Invalid file format or magic number\n";
                break;
            case DataResult::kErrorVersionMismatch:
                std::cerr << "  Reason: File version mismatch\n";
                break;
            default:
                std::cerr << "  Reason: Unknown error\n";
                break;
        }
        return;
    }
    
    std::cout << "  Loaded " << lib_map.size() << " libraries\n";
    
    // 2. Load call stack data
    constexpr size_t kMaxDepth = 128;
    constexpr size_t kCapacity = 10000;
    using CallStackType = CallStack<kMaxDepth>;
    using HashMapType = StaticHashMap<CallStackType, uint64_t, kCapacity>;
    
    DataImporter data_importer(data_file);
    HashMapType call_stacks;
    
    DataResult data_result = data_importer.importData(call_stacks);
    if (data_result != DataResult::kSuccess) {
        std::cerr << "Failed to import call stack data for rank " << rank << "\n";
        std::cerr << "  File: " << data_file << "\n";
        std::cerr << "  Error code: " << static_cast<int>(data_result) << "\n";
        
        // Provide helpful error messages based on error code
        switch (data_result) {
            case DataResult::kErrorFileOpen:
                std::cerr << "  Reason: File not found or cannot be opened\n";
                std::cerr << "  Hint: Make sure you have run the MPI sampler first to generate .pflw files\n";
                break;
            case DataResult::kErrorFileRead:
                std::cerr << "  Reason: Error reading file\n";
                break;
            case DataResult::kErrorInvalidFormat:
                std::cerr << "  Reason: Invalid file format or magic number\n";
                break;
            case DataResult::kErrorVersionMismatch:
                std::cerr << "  Reason: File version mismatch\n";
                break;
            case DataResult::kErrorMemory:
                std::cerr << "  Reason: Out of memory or capacity exceeded\n";
                break;
            default:
                std::cerr << "  Reason: Unknown error\n";
                break;
        }
        return;
    }
    
    std::cout << "  Loaded " << call_stacks.size() << " unique call stacks\n";
    
    // 3. Create offset converter and add map snapshot
    OffsetConverter converter;
    converter.add_map_snapshot(rank, lib_map);
    
    // 4. Process each call stack
    std::cout << "\nConverting raw addresses to offsets...\n";
    
    // Count statistics
    size_t total_samples = 0;
    std::map<std::string, uint64_t> library_hotness;  // Library name -> sample count
    
    call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
        total_samples += count;
        
        // Convert raw addresses to resolved frames
        std::vector<ResolvedFrame> resolved = converter.convert(stack, rank);
        
        // Aggregate statistics by library
        for (const auto& frame : resolved) {
            library_hotness[frame.library_name] += count;
        }
    });
    
    std::cout << "  Total samples: " << total_samples << "\n";
    std::cout << "\nHotness by library:\n";
    
    // Sort libraries by sample count
    std::vector<std::pair<std::string, uint64_t>> sorted_libs(
        library_hotness.begin(), library_hotness.end());
    std::sort(sorted_libs.begin(), sorted_libs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Print top 10 libraries
    size_t count = 0;
    for (const auto& [lib_name, samples] : sorted_libs) {
        double percentage = 100.0 * samples / total_samples;
        std::cout << "  " << lib_name << ": " 
                  << samples << " samples (" << percentage << "%)\n";
        if (++count >= 10) break;
    }
}

/// Example: Process data from multiple ranks
void process_all_ranks(const char* data_dir, int num_ranks) {
    std::cout << "Processing data from " << num_ranks << " ranks\n\n";
    
    for (int rank = 0; rank < num_ranks; ++rank) {
        process_rank_data(data_dir, static_cast<uint32_t>(rank));
        std::cout << "\n" << std::string(60, '-') << "\n\n";
    }
}

/// Example: Advanced analysis - find hot paths
void find_hot_paths(const char* data_dir, uint32_t rank, size_t top_n) {
    // Build filenames
    char data_file[256];
    char libmap_file[256];
    std::snprintf(data_file, sizeof(data_file), 
                  "%s/perflow_mpi_rank_%u.pflw", data_dir, rank);
    std::snprintf(libmap_file, sizeof(libmap_file),
                  "%s/perflow_mpi_rank_%u.libmap", data_dir, rank);
    
    // Load library map
    LibraryMapImporter map_importer(libmap_file);
    LibraryMap lib_map;
    map_importer.importMap(lib_map, nullptr);
    
    // Load call stack data
    constexpr size_t kMaxDepth = 128;
    constexpr size_t kCapacity = 10000;
    using CallStackType = CallStack<kMaxDepth>;
    using HashMapType = StaticHashMap<CallStackType, uint64_t, kCapacity>;
    
    DataImporter data_importer(data_file);
    HashMapType call_stacks;
    data_importer.importData(call_stacks);
    
    // Create offset converter
    OffsetConverter converter;
    converter.add_map_snapshot(rank, lib_map);
    
    // Find top N hottest call stacks
    std::vector<std::pair<CallStackType, uint64_t>> stack_counts;
    
    call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
        stack_counts.emplace_back(stack, count);
    });
    
    // Sort by count
    std::sort(stack_counts.begin(), stack_counts.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Print top N
    std::cout << "Top " << top_n << " hottest call stacks:\n\n";
    
    for (size_t i = 0; i < std::min(top_n, stack_counts.size()); ++i) {
        const auto& [stack, count] = stack_counts[i];
        
        std::cout << "Call Stack #" << (i + 1) << " (samples: " << count << ")\n";
        
        // Convert and print resolved frames
        std::vector<ResolvedFrame> resolved = converter.convert(stack, rank);
        
        for (size_t j = 0; j < resolved.size(); ++j) {
            const auto& frame = resolved[j];
            std::cout << "  Frame " << j << ": "
                      << frame.library_name << " + 0x" 
                      << std::hex << frame.offset << std::dec
                      << " (raw: 0x" << std::hex << frame.raw_address << std::dec << ")\n";
        }
        
        std::cout << "\n";
    }
}

/// Main function - demonstrates different usage patterns
int main(int argc, char* argv[]) {
    std::cout << "PerFlow Post-Analysis Example\n";
    std::cout << "==============================\n\n";
    
    const char* data_dir = "/tmp";
    
    // Allow user to specify data directory
    if (argc > 1) {
        data_dir = argv[1];
    }
    
    std::cout << "Data directory: " << data_dir << "\n";
    std::cout << "Looking for files:\n";
    std::cout << "  - perflow_mpi_rank_N.pflw (sample data)\n";
    std::cout << "  - perflow_mpi_rank_N.libmap (library maps)\n\n";
    
    // Example 1: Process a single rank
    std::cout << "=== Example 1: Process Single Rank ===\n\n";
    process_rank_data(data_dir, 0);
    
    std::cout << "\n" << std::string(60, '=') << "\n\n";
    
    // Example 2: Process all ranks
    std::cout << "=== Example 2: Process All Ranks ===\n\n";
    std::cout << "Uncomment in source to process multiple ranks\n";
    // process_all_ranks(data_dir, 4);  // Uncomment if you have data from 4 ranks
    
    std::cout << "\n" << std::string(60, '=') << "\n\n";
    
    // Example 3: Find hot paths
    std::cout << "=== Example 3: Find Hot Paths ===\n\n";
    std::cout << "Uncomment in source to find hot call paths\n";
    // find_hot_paths(data_dir, 0, 5);  // Uncomment to find top 5 hot paths
    
    std::cout << "\n" << std::string(60, '=') << "\n\n";
    std::cout << "Usage: " << (argc > 0 ? argv[0] : "post_analysis_example") 
              << " [data_directory]\n";
    std::cout << "\nTo generate sample data, run:\n";
    std::cout << "  LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \\\n";
    std::cout << "  PERFLOW_OUTPUT_DIR=" << data_dir << " \\\n";
    std::cout << "  mpirun -n 4 ./your_mpi_app\n\n";
    
    return 0;
}
