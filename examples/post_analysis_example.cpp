// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file post_analysis_example.cpp
/// @brief Example demonstrating post-analysis conversion of raw addresses to offsets
///
/// This example shows how to:
/// 1. Import library maps from files
/// 2. Import call stack data from sampling files
/// 3. Use OffsetConverter to convert raw addresses to resolved frames
/// 4. Build performance trees from the data
/// 5. Generate PDF visualizations of the call tree
/// 6. Process and analyze the resolved frames
///
/// Usage:
///   // Assuming you have collected data with the MPI sampler
///   // The sampler will generate files like:
///   //   - perflow_mpi_rank_0.pflw (call stack data)
///   //   - perflow_mpi_rank_0.libmap (library map)
///   //   - perflow_mpi_rank_0.txt (human-readable data)
///
///   // This example shows how to process them in post-analysis
///   // and generate PDF visualizations

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/library_map.h"
#include "sampling/static_hash_map.h"
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"
#include "analysis/performance_tree.h"
#include "analysis/tree_builder.h"
#include "analysis/tree_visualizer.h"

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
    
    // 3. Create symbol resolver for function name resolution
    std::cout << "\nCreating symbol resolver...\n";
    auto resolver = std::make_shared<SymbolResolver>(
        SymbolResolver::Strategy::kAutoFallback,  // Try dladdr first, fallback to addr2line
        true  // Enable caching
    );
    std::cout << "  Symbol resolver created with auto-fallback strategy\n";
    
    // 4. Create offset converter with symbol resolver and add map snapshot
    OffsetConverter converter(resolver);
    converter.add_map_snapshot(rank, lib_map);
    std::cout << "  Offset converter configured with symbol resolution\n";
    
    // 5. Build performance tree for visualization
    std::cout << "\nBuilding performance tree...\n";
    PerformanceTree tree;
    tree.set_process_count(1);  // Single rank analysis
    
    call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
        // Convert raw addresses to resolved frames with symbol resolution
        std::vector<ResolvedFrame> resolved = converter.convert(stack, rank, true);
        
        // Insert into tree (frames are already in bottom-to-top order)
        tree.insert_call_stack(resolved, rank, count, count * 1000.0);  // Assume 1ms per sample
    });
    
    std::cout << "  Tree built with " << tree.total_samples() << " total samples\n";
    
    // 6. Generate PDF visualization
    std::cout << "\nGenerating PDF visualization...\n";
    
    char pdf_path[512];
    std::snprintf(pdf_path, sizeof(pdf_path), "%s/performance_tree_rank_%u.pdf", 
                  data_dir, rank);
    
    bool viz_success = TreeVisualizer::generate_pdf(tree, pdf_path, 
                                                     ColorScheme::kHeatmap, 15);
    
    if (viz_success) {
        std::cout << "  PDF saved to: " << pdf_path << "\n";
    } else {
        std::cout << "  PDF generation failed (GraphViz may not be installed)\n";
        std::cout << "  Install with: sudo apt-get install graphviz\n";
    }
    
    // 7. Process statistics with symbol resolution
    std::cout << "\nConverting raw addresses to offsets and resolving symbols...\n";
    
    // Count statistics
    size_t total_samples = 0;
    std::map<std::string, uint64_t> library_hotness;  // Library name -> sample count
    std::map<std::string, uint64_t> function_hotness;  // Function name -> sample count
    
    call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
        total_samples += count;
        
        // Convert raw addresses to resolved frames with symbols
        std::vector<ResolvedFrame> resolved = converter.convert(stack, rank, true);
        
        // Aggregate statistics by library and function
        for (const auto& frame : resolved) {
            library_hotness[frame.library_name] += count;
            
            // If function name was resolved, track it
            if (!frame.function_name.empty()) {
                std::string func_key = frame.function_name;
                if (!frame.filename.empty()) {
                    func_key += " (" + frame.filename;
                    if (frame.line_number > 0) {
                        func_key += ":" + std::to_string(frame.line_number);
                    }
                    func_key += ")";
                }
                function_hotness[func_key] += count;
            }
        }
    });
    
    std::cout << "  Total samples: " << total_samples << "\n";
    
    // Display symbol resolution statistics
    auto cache_stats = resolver->get_cache_stats();
    std::cout << "\nSymbol Resolution Cache Statistics:\n";
    std::cout << "  Cache hits: " << cache_stats.hits << "\n";
    std::cout << "  Cache misses: " << cache_stats.misses << "\n";
    std::cout << "  Cache size: " << cache_stats.size << " symbols\n";
    if (cache_stats.hits + cache_stats.misses > 0) {
        double hit_rate = 100.0 * cache_stats.hits / (cache_stats.hits + cache_stats.misses);
        std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) << hit_rate << "%\n";
    }
    
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
    
    // Display hottest functions if any were resolved
    if (!function_hotness.empty()) {
        std::cout << "\nHotness by function (top 10):\n";
        
        // Sort functions by sample count
        std::vector<std::pair<std::string, uint64_t>> sorted_funcs(
            function_hotness.begin(), function_hotness.end());
        std::sort(sorted_funcs.begin(), sorted_funcs.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        count = 0;
        for (const auto& [func_name, samples] : sorted_funcs) {
            double percentage = 100.0 * samples / total_samples;
            std::cout << "  " << func_name << ": " 
                      << samples << " samples (" << percentage << "%)\n";
            if (++count >= 10) break;
        }
    }
}

/// Example: Process data from multiple ranks using TreeBuilder
void process_all_ranks(const char* data_dir, int num_ranks) {
    std::cout << "Processing data from " << num_ranks << " ranks into a single tree\n\n";
    
    // Create symbol resolver for function name and location resolution
    std::cout << "Creating symbol resolver...\n";
    auto resolver = std::make_shared<SymbolResolver>(
        SymbolResolver::Strategy::kAutoFallback,  // Try dladdr first, fallback to addr2line
        true  // Enable caching
    );
    std::cout << "  Symbol resolver created with auto-fallback strategy\n\n";
    
    // Create TreeBuilder with symbol resolver
    TreeBuilder builder(resolver);
    
    // 1. Load library maps for all ranks
    std::cout << "Loading library maps...\n";
    std::vector<std::pair<std::string, uint32_t>> libmap_files;
    for (int rank = 0; rank < num_ranks; ++rank) {
        char libmap_file[512];
        std::snprintf(libmap_file, sizeof(libmap_file),
                      "%s/perflow_mpi_rank_%d.libmap", data_dir, rank);
        libmap_files.emplace_back(libmap_file, rank);
    }
    
    size_t libmaps_loaded = builder.load_library_maps(libmap_files);
    std::cout << "  Loaded " << libmaps_loaded << " / " << num_ranks 
              << " library maps\n";
    
    // 2. Build tree from all sample files
    std::cout << "\nBuilding unified performance tree from all ranks...\n";
    std::vector<std::pair<std::string, uint32_t>> sample_files;
    for (int rank = 0; rank < num_ranks; ++rank) {
        char data_file[512];
        std::snprintf(data_file, sizeof(data_file),
                      "%s/perflow_mpi_rank_%d.pflw", data_dir, rank);
        sample_files.emplace_back(data_file, rank);
    }
    
    size_t files_loaded = builder.build_from_files(sample_files, 1000.0);
    std::cout << "  Processed " << files_loaded << " / " << num_ranks 
              << " sample files\n";
    std::cout << "  Total samples in tree: " << builder.tree().total_samples() << "\n";
    std::cout << "  Process count: " << builder.tree().process_count() << "\n";
    
    // 3. Generate PDF visualization of the unified tree
    std::cout << "\nGenerating PDF visualization of unified tree...\n";
    
    char pdf_path[512];
    std::snprintf(pdf_path, sizeof(pdf_path), 
                  "%s/performance_tree_all_ranks.pdf", data_dir);
    
    bool viz_success = TreeVisualizer::generate_pdf(builder.tree(), pdf_path,
                                                     ColorScheme::kHeatmap, 15);
    
    if (viz_success) {
        std::cout << "  PDF saved to: " << pdf_path << "\n";
    } else {
        std::cout << "  PDF generation failed (GraphViz may not be installed)\n";
        std::cout << "  Install with: sudo apt-get install graphviz\n";
    }
    
    // 4. Print some statistics
    std::cout << "\nTree Statistics:\n";
    std::cout << "  Root node children: " << builder.tree().root()->children().size() << "\n";
    
    // Print top-level functions by sample count
    auto root_children = builder.tree().root()->children();
    if (!root_children.empty()) {
        std::vector<std::pair<std::shared_ptr<TreeNode>, uint64_t>> sorted_children;
        for (const auto& child : root_children) {
            sorted_children.emplace_back(child, child->total_samples());
        }
        std::sort(sorted_children.begin(), sorted_children.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::cout << "\n  Top 5 functions (by total samples):\n";
        size_t count = 0;
        for (const auto& [node, samples] : sorted_children) {
            double percentage = 100.0 * samples / builder.tree().total_samples();
            std::cout << "    " << node->frame().function_name 
                      << ": " << samples << " samples (" 
                      << percentage << "%)\n";
            if (++count >= 5) break;
        }
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
    
    // Create symbol resolver
    auto resolver = std::make_shared<SymbolResolver>(
        SymbolResolver::Strategy::kAutoFallback,
        true  // Enable caching
    );
    
    // Create offset converter with symbol resolver
    OffsetConverter converter(resolver);
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
        
        // Convert and print resolved frames with symbols
        std::vector<ResolvedFrame> resolved = converter.convert(stack, rank, true);
        
        for (size_t j = 0; j < resolved.size(); ++j) {
            const auto& frame = resolved[j];
            std::cout << "  Frame " << j << ": "
                      << frame.library_name << " + 0x" 
                      << std::hex << frame.offset << std::dec;
            
            if (!frame.function_name.empty()) {
                std::cout << " [" << frame.function_name << "]";
            }
            
            if (!frame.filename.empty()) {
                std::cout << "\n             at " << frame.filename;
                if (frame.line_number > 0) {
                    std::cout << ":" << frame.line_number;
                }
            }
            
            std::cout << "\n             (raw: 0x" << std::hex << frame.raw_address << std::dec << ")\n";
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
    
    // Example 2: Process all ranks into a unified tree
    std::cout << "=== Example 2: Process All Ranks (Unified Tree) ===\n\n";
    
    // Check if we have multiple rank files by trying to open rank 1
    char test_file[512];
    std::snprintf(test_file, sizeof(test_file), 
                  "%s/perflow_mpi_rank_1.pflw", data_dir);
    FILE* fp = fopen(test_file, "rb");
    if (fp) {
        fclose(fp);
        // We have at least rank 0 and rank 1, try to process multiple ranks
        // Detect how many ranks we have (up to 16 for this example)
        int num_ranks = 1;
        for (int i = 1; i < 16; ++i) {
            std::snprintf(test_file, sizeof(test_file),
                          "%s/perflow_mpi_rank_%d.pflw", data_dir, i);
            FILE* test_fp = fopen(test_file, "rb");
            if (test_fp) {
                fclose(test_fp);
                num_ranks = i + 1;
            } else {
                break;
            }
        }
        std::cout << "Detected " << num_ranks << " rank(s)\n\n";
        process_all_ranks(data_dir, num_ranks);
    } else {
        std::cout << "Only single rank data found. Run with multiple MPI ranks to see unified tree.\n";
        std::cout << "To generate multi-rank data:\n";
        std::cout << "  LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \\\n";
        std::cout << "  PERFLOW_OUTPUT_DIR=" << data_dir << " \\\n";
        std::cout << "  mpirun -n 4 ./your_mpi_app\n";
    }
    
    std::cout << "\n" << std::string(60, '=') << "\n\n";
    
    // Example 3: Find hot paths
    std::cout << "=== Example 3: Find Hot Paths ===\n\n";
    std::cout << "Uncomment in source to find hot call paths\n";
    // find_hot_paths(data_dir, 0, 5);  // Uncomment to find top 5 hot paths
    
    std::cout << "\n" << std::string(60, '=') << "\n\n";
    std::cout << "Usage: " << (argc > 0 ? argv[0] : "post_analysis_example") 
              << " [data_directory]\n";
    std::cout << "\nOutput files:\n";
    std::cout << "  - " << data_dir << "/performance_tree_rank_N.pdf (per-rank trees)\n";
    std::cout << "  - " << data_dir << "/performance_tree_all_ranks.pdf (unified tree)\n";
    std::cout << "  - Library hotness statistics (console output)\n\n";
    std::cout << "Note: PDF generation requires GraphViz to be installed\n";
    std::cout << "      Install with: sudo apt-get install graphviz\n\n";
    
    return 0;
}
