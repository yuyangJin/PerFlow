// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file offset_converter_example.cpp
/// @brief Example demonstrating the usage of LibraryMap and OffsetConverter
///
/// This example shows how to:
/// 1. Capture library maps from the current process
/// 2. Store raw call stack addresses with metadata
/// 3. Post-process addresses to resolve them to (library, offset) pairs

#include <iostream>
#include <chrono>

#include "sampling/library_map.h"
#include "sampling/call_stack.h"
#include "analysis/offset_converter.h"

using namespace perflow::sampling;
using namespace perflow::analysis;

/// Example function to simulate capturing a call stack
/// In real usage, this would use libunwind or other stack capture mechanisms
CallStack<> simulate_capture_call_stack() {
    CallStack<> stack;
    
    // These would be real addresses in a production scenario
    // For demonstration, we use placeholder addresses
    stack.push(0x7f8a4c010000);
    stack.push(0x7f8a4c015000);
    stack.push(0x7ffff7dd6000);
    
    return stack;
}

/// Get current timestamp in nanoseconds
int64_t get_timestamp_ns() {
    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch());
    return ns.count();
}

int main() {
    std::cout << "=== PerFlow: Call Stack Address Offset Conversion Example ===" << std::endl;
    std::cout << std::endl;
    
    // Step 1: Capture library map at initialization
    std::cout << "Step 1: Capturing library map from current process..." << std::endl;
    LibraryMap initial_map;
    if (!initial_map.parse_current_process()) {
        std::cerr << "Error: Failed to parse /proc/self/maps" << std::endl;
        return 1;
    }
    std::cout << "  Captured " << initial_map.size() << " executable regions" << std::endl;
    std::cout << std::endl;
    
    // Step 2: Create offset converter and add the library map snapshot
    std::cout << "Step 2: Creating OffsetConverter and adding map snapshot..." << std::endl;
    OffsetConverter converter;
    converter.add_map_snapshot(0, initial_map);
    std::cout << "  Map snapshot ID 0 added" << std::endl;
    std::cout << std::endl;
    
    // Step 3: Simulate capturing call stacks during execution
    std::cout << "Step 3: Capturing call stacks with raw addresses..." << std::endl;
    std::vector<RawCallStack<>> raw_stacks;
    
    for (int i = 0; i < 3; ++i) {
        CallStack<> stack = simulate_capture_call_stack();
        int64_t timestamp = get_timestamp_ns();
        uint32_t map_id = 0;  // Use initial map for all samples
        
        raw_stacks.emplace_back(stack, timestamp, map_id);
        std::cout << "  Captured stack #" << i << " with " << stack.depth() 
                  << " frames at timestamp " << timestamp << std::endl;
    }
    std::cout << std::endl;
    
    // Step 4: Post-process to resolve addresses to (library, offset) pairs
    std::cout << "Step 4: Post-processing - resolving addresses to offsets..." << std::endl;
    std::vector<ResolvedCallStack> resolved_stacks = converter.convert_batch(raw_stacks);
    std::cout << "  Resolved " << resolved_stacks.size() << " call stacks" << std::endl;
    std::cout << std::endl;
    
    // Step 5: Display results
    std::cout << "Step 5: Displaying resolved call stacks..." << std::endl;
    for (size_t i = 0; i < resolved_stacks.size(); ++i) {
        const auto& resolved = resolved_stacks[i];
        std::cout << "\n  Call Stack #" << i << ":" << std::endl;
        std::cout << "    Timestamp: " << resolved.timestamp << " ns" << std::endl;
        std::cout << "    Map ID: " << resolved.map_id << std::endl;
        std::cout << "    Frames: " << resolved.frames.size() << std::endl;
        
        for (size_t j = 0; j < resolved.frames.size(); ++j) {
            const auto& frame = resolved.frames[j];
            std::cout << "      Frame " << j << ": " << frame.library_name 
                      << " + 0x" << std::hex << frame.offset << std::dec
                      << " (raw: 0x" << std::hex << frame.raw_address << std::dec << ")"
                      << std::endl;
        }
    }
    std::cout << std::endl;
    
    // Step 6: Example of resolving a single address
    std::cout << "Step 6: Example of resolving a single address..." << std::endl;
    uintptr_t test_addr = 0x7f8a4c010000;
    auto result = initial_map.resolve(test_addr);
    if (result.has_value()) {
        std::cout << "  Address 0x" << std::hex << test_addr << std::dec
                  << " resolves to:" << std::endl;
        std::cout << "    Library: " << result->first << std::endl;
        std::cout << "    Offset: 0x" << std::hex << result->second << std::dec << std::endl;
    } else {
        std::cout << "  Address 0x" << std::hex << test_addr << std::dec
                  << " could not be resolved (not in any loaded library)" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "=== Example Complete ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Key Points:" << std::endl;
    std::cout << "  - Library maps are captured once at initialization (minimal overhead)" << std::endl;
    std::cout << "  - Raw addresses are stored during sampling (fast)" << std::endl;
    std::cout << "  - Address resolution happens during post-processing (no runtime impact)" << std::endl;
    std::cout << "  - Supports multiple map snapshots for dynamic library loading" << std::endl;
    
    return 0;
}
