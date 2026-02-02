// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file npb_hotspot_analyzer.cpp
/// @brief Hotspot analysis tool for NPB benchmarks
///
/// This tool analyzes PerFlow sample data and outputs hotspot information in JSON format.
/// It is designed to be used by the NPB benchmark test scripts.
///
/// Usage:
///   npb_hotspot_analyzer <data_dir> <output_json> [num_ranks]
///
/// Example:
///   npb_hotspot_analyzer ./sample_data hotspot_output.json 4

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstring>

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/library_map.h"
#include "sampling/static_hash_map.h"
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"

using namespace perflow::sampling;
using namespace perflow::analysis;

// Functions to exclude from hotspot analysis
static const std::set<std::string> EXCLUDED_PATTERNS = {
    "_start",
    "__libc_start_main",
    "main",
    "PAPI_",
    "papi_",
    "_papi_",
    "perflow_",
    "__perflow_"
};

/// Check if a function should be excluded from analysis
bool should_exclude_function(const std::string& func_name) {
    for (const auto& pattern : EXCLUDED_PATTERNS) {
        if (func_name.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// Structure to hold hotspot information
struct HotspotInfo {
    std::string function_name;
    std::string library_name;
    std::string filename;
    int line_number;
    uint64_t exclusive_samples;  // Samples where this is the leaf function
    uint64_t inclusive_samples;  // Samples where this appears in the stack
    double exclusive_percentage;
    double inclusive_percentage;
};

/// Analyze hotspots from sample data
/// @param data_dir Directory containing sample data files
/// @param num_ranks Number of MPI ranks to process
/// @param total_samples Output parameter to store the total number of samples
/// @return Vector of hotspot information sorted by exclusive samples
std::vector<HotspotInfo> analyze_hotspots(const char* data_dir, int num_ranks, uint64_t& total_samples) {
    std::map<std::string, HotspotInfo> hotspot_map;
    total_samples = 0;
    
    // Process each rank
    for (int rank = 0; rank < num_ranks; ++rank) {
        // Build filenames
        char data_file[512];
        char libmap_file[512];
        std::snprintf(data_file, sizeof(data_file), 
                      "%s/perflow_mpi_rank_%d.pflw", data_dir, rank);
        std::snprintf(libmap_file, sizeof(libmap_file),
                      "%s/perflow_mpi_rank_%d.libmap", data_dir, rank);
        
        // Load library map
        LibraryMapImporter map_importer(libmap_file);
        LibraryMap lib_map;
        uint32_t process_id = 0;
        
        DataResult map_result = map_importer.importMap(lib_map, &process_id);
        if (map_result != DataResult::kSuccess) {
            std::cerr << "Warning: Failed to import library map for rank " << rank 
                      << " (file: " << libmap_file << ")\n";
            continue;
        }
        
        // Load call stack data
        constexpr size_t kMaxDepth = 128;
        constexpr size_t kCapacity = 100000;  // Increased capacity
        using CallStackType = CallStack<kMaxDepth>;
        using HashMapType = StaticHashMap<CallStackType, uint64_t, kCapacity>;
        
        DataImporter data_importer(data_file);
        HashMapType call_stacks;
        
        DataResult data_result = data_importer.importData(call_stacks);
        if (data_result != DataResult::kSuccess) {
            std::cerr << "Warning: Failed to import call stack data for rank " << rank
                      << " (file: " << data_file << ")\n";
            continue;
        }
        
        // Create symbol resolver
        auto resolver = std::make_shared<SymbolResolver>(
            SymbolResolver::Strategy::kAutoFallback,
            true  // Enable caching
        );
        
        // Create offset converter with symbol resolver
        OffsetConverter converter(resolver);
        converter.add_map_snapshot(rank, lib_map);
        
        // Analyze call stacks
        call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
            total_samples += count;
            
            // Convert raw addresses to resolved frames with symbols
            std::vector<ResolvedFrame> resolved = converter.convert(stack, rank, true);
            
            if (resolved.empty()) return;
            
            // Track which functions appeared in this stack (for inclusive count)
            std::set<std::string> functions_in_stack;
            
            // Process each frame in the stack
            for (size_t i = 0; i < resolved.size(); ++i) {
                const auto& frame = resolved[i];
                
                // Skip if no function name or should be excluded
                if (frame.function_name.empty() || 
                    should_exclude_function(frame.function_name)) {
                    continue;
                }
                
                // Create unique key for this function
                std::string func_key = frame.function_name;
                
                // Add to inclusive count (only once per stack)
                if (functions_in_stack.find(func_key) == functions_in_stack.end()) {
                    functions_in_stack.insert(func_key);
                    
                    auto& hotspot = hotspot_map[func_key];
                    if (hotspot.function_name.empty()) {
                        hotspot.function_name = frame.function_name;
                        hotspot.library_name = frame.library_name;
                        hotspot.filename = frame.filename;
                        hotspot.line_number = frame.line_number;
                        hotspot.exclusive_samples = 0;
                        hotspot.inclusive_samples = 0;
                    }
                    hotspot.inclusive_samples += count;
                }
                
                // For exclusive samples, only count leaf function (first in resolved stack)
                if (i == 0) {
                    auto& hotspot = hotspot_map[func_key];
                    hotspot.exclusive_samples += count;
                }
            }
        });
    }
    
    // Calculate percentages
    for (auto& [key, hotspot] : hotspot_map) {
        hotspot.exclusive_percentage = (total_samples > 0) ? 
            (100.0 * hotspot.exclusive_samples / total_samples) : 0.0;
        hotspot.inclusive_percentage = (total_samples > 0) ? 
            (100.0 * hotspot.inclusive_samples / total_samples) : 0.0;
    }
    
    // Convert to vector and sort by exclusive samples
    std::vector<HotspotInfo> hotspots;
    for (const auto& [key, hotspot] : hotspot_map) {
        hotspots.push_back(hotspot);
    }
    
    std::sort(hotspots.begin(), hotspots.end(),
              [](const HotspotInfo& a, const HotspotInfo& b) {
                  return a.exclusive_samples > b.exclusive_samples;
              });
    
    return hotspots;
}

/// Write hotspots to JSON file
void write_json_output(const std::vector<HotspotInfo>& hotspots, 
                       const char* output_file,
                       uint64_t total_samples) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open output file: " << output_file << "\n";
        return;
    }
    
    out << "{\n";
    out << "  \"total_samples\": " << total_samples << ",\n";
    out << "  \"hotspots\": [\n";
    
    for (size_t i = 0; i < hotspots.size(); ++i) {
        const auto& h = hotspots[i];
        
        out << "    {\n";
        out << "      \"function\": \"" << h.function_name << "\",\n";
        out << "      \"library\": \"" << h.library_name << "\",\n";
        out << "      \"filename\": \"" << h.filename << "\",\n";
        out << "      \"line_number\": " << h.line_number << ",\n";
        out << "      \"exclusive_samples\": " << h.exclusive_samples << ",\n";
        out << "      \"inclusive_samples\": " << h.inclusive_samples << ",\n";
        out << "      \"exclusive_percentage\": " << std::fixed << std::setprecision(2) 
            << h.exclusive_percentage << ",\n";
        out << "      \"inclusive_percentage\": " << std::fixed << std::setprecision(2) 
            << h.inclusive_percentage << "\n";
        out << "    }";
        
        if (i < hotspots.size() - 1) {
            out << ",";
        }
        out << "\n";
    }
    
    out << "  ]\n";
    out << "}\n";
    
    out.close();
    std::cout << "Hotspot data written to: " << output_file << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <data_dir> <output_json> [num_ranks]\n";
        std::cerr << "\n";
        std::cerr << "Arguments:\n";
        std::cerr << "  data_dir     Directory containing perflow_mpi_rank_*.pflw files\n";
        std::cerr << "  output_json  Output JSON file for hotspot data\n";
        std::cerr << "  num_ranks    Number of MPI ranks (default: auto-detect)\n";
        std::cerr << "\n";
        std::cerr << "Example:\n";
        std::cerr << "  " << argv[0] << " ./sample_data hotspot_output.json 4\n";
        return 1;
    }
    
    const char* data_dir = argv[1];
    const char* output_file = argv[2];
    int num_ranks = 1;
    
    // Auto-detect number of ranks if not provided
    if (argc > 3) {
        num_ranks = std::atoi(argv[3]);
    } else {
        // Try to detect number of ranks by checking for files
        for (int i = 0; i < 1024; ++i) {
            char test_file[512];
            std::snprintf(test_file, sizeof(test_file),
                          "%s/perflow_mpi_rank_%d.pflw", data_dir, i);
            FILE* fp = fopen(test_file, "rb");
            if (fp) {
                fclose(fp);
                num_ranks = i + 1;
            } else {
                break;
            }
        }
    }
    
    std::cout << "NPB Hotspot Analyzer\n";
    std::cout << "====================\n";
    std::cout << "Data directory: " << data_dir << "\n";
    std::cout << "Output file: " << output_file << "\n";
    std::cout << "Number of ranks: " << num_ranks << "\n\n";
    
    // Analyze hotspots
    std::cout << "Analyzing hotspots...\n";
    uint64_t total_samples = 0;
    std::vector<HotspotInfo> hotspots = analyze_hotspots(data_dir, num_ranks, total_samples);
    
    if (hotspots.empty()) {
        std::cerr << "Warning: No hotspots found. Check that sample data files exist.\n";
        return 1;
    }
    
    std::cout << "Found " << hotspots.size() << " unique functions\n";
    std::cout << "Total samples: " << total_samples << "\n\n";
    
    // Display top 10 hotspots
    std::cout << "Top 10 hotspots (by exclusive samples):\n";
    for (size_t i = 0; i < std::min(size_t(10), hotspots.size()); ++i) {
        const auto& h = hotspots[i];
        std::cout << "  " << (i + 1) << ". " << h.function_name 
                  << ": " << h.exclusive_samples << " samples (" 
                  << std::fixed << std::setprecision(2) 
                  << h.exclusive_percentage << "%)\n";
    }
    std::cout << "\n";
    
    // Write JSON output
    write_json_output(hotspots, output_file, total_samples);
    
    return 0;
}
