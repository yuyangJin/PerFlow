// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file symbol_resolution_example.cpp
/// @brief Example demonstrating the usage of SymbolResolver with OffsetConverter
///
/// This example shows how to:
/// 1. Create and configure a SymbolResolver
/// 2. Use OffsetConverter with symbol resolution enabled
/// 3. Resolve function names and source locations from raw addresses

#include <iostream>
#include <iomanip>
#include <memory>
#include <cstdlib>
#include <dlfcn.h>

#include "sampling/library_map.h"
#include "sampling/call_stack.h"
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"

using namespace perflow::sampling;
using namespace perflow::analysis;

// Example function to demonstrate symbol resolution
__attribute__((noinline)) void example_function_level_3() {
  // Some work here
  volatile int x = 42;
  (void)x;
}

__attribute__((noinline)) void example_function_level_2() {
  example_function_level_3();
}

__attribute__((noinline)) void example_function_level_1() {
  example_function_level_2();
}

void print_resolved_frame(const ResolvedFrame& frame, size_t index) {
  std::cout << "    Frame " << index << ":" << std::endl;
  std::cout << "      Raw Address: 0x" << std::hex << frame.raw_address << std::dec << std::endl;
  std::cout << "      Library: " << frame.library_name << std::endl;
  std::cout << "      Offset: 0x" << std::hex << frame.offset << std::dec << std::endl;
  
  if (!frame.function_name.empty()) {
    std::cout << "      Function: " << frame.function_name << std::endl;
  } else {
    std::cout << "      Function: [not resolved]" << std::endl;
  }
  
  if (!frame.filename.empty()) {
    std::cout << "      Source: " << frame.filename;
    if (frame.line_number > 0) {
      std::cout << ":" << frame.line_number;
    }
    std::cout << std::endl;
  } else if (frame.function_name.empty()) {
    // Only print debug info if nothing was resolved
    std::cout << "      [Debug: Try 'addr2line -e " << frame.library_name 
              << " -f -C 0x" << std::hex << frame.offset << std::dec << "']" << std::endl;
  }
}


int main() {
  std::cout << "=== PerFlow: Symbol Resolution Example ===" << std::endl;
  std::cout << std::endl;
  
  // Step 1: Capture library map at initialization
  std::cout << "Step 1: Capturing library map from current process..." << std::endl;
  LibraryMap lib_map;
  if (!lib_map.parse_current_process()) {
    std::cerr << "Error: Failed to parse /proc/self/maps" << std::endl;
    return 1;
  }
  std::cout << "  Captured " << lib_map.size() << " executable regions" << std::endl;
  std::cout << std::endl;
  
  // Step 2: Create a SymbolResolver
  std::cout << "Step 2: Creating SymbolResolver..." << std::endl;
  
  // Check for debug mode via environment variable
  bool debug_mode = (std::getenv("PERFLOW_SYMBOL_DEBUG") != nullptr);
  if (debug_mode) {
    std::cout << "  *** DEBUG MODE ENABLED (PERFLOW_SYMBOL_DEBUG is set) ***" << std::endl;
  }
  
  auto resolver = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kAutoFallback,  // Try dladdr first, fallback to addr2line
      true,  // Enable caching
      debug_mode  // Enable debug output if env var is set
  );
  std::cout << "  Symbol resolver created with auto-fallback strategy" << std::endl;
  std::cout << std::endl;
  
  // Step 3: Create OffsetConverter with symbol resolver
  std::cout << "Step 3: Creating OffsetConverter with symbol resolution..." << std::endl;
  OffsetConverter converter(resolver);
  converter.add_map_snapshot(0, lib_map);
  std::cout << "  Converter initialized with symbol resolution enabled" << std::endl;
  std::cout << std::endl;
  
  // Step 4: Get addresses from actual functions
  std::cout << "Step 4: Resolving actual function addresses..." << std::endl;
  
  // Get address of known libc function
  void* printf_addr = dlsym(RTLD_DEFAULT, "printf");
  if (printf_addr) {
    Dl_info info;
    if (dladdr(printf_addr, &info) != 0) {
      uintptr_t addr = reinterpret_cast<uintptr_t>(printf_addr);
      
      CallStack<> stack;
      stack.push(addr);
      
      std::cout << "  Example 1: Resolving 'printf' from libc" << std::endl;
      
      // Convert WITHOUT symbol resolution
      auto resolved_no_symbols = converter.convert(stack, 0, false);
      std::cout << "    Without symbol resolution:" << std::endl;
      print_resolved_frame(resolved_no_symbols[0], 0);
      std::cout << std::endl;
      
      // Convert WITH symbol resolution
      auto resolved_with_symbols = converter.convert(stack, 0, true);
      std::cout << "    With symbol resolution:" << std::endl;
      print_resolved_frame(resolved_with_symbols[0], 0);
      std::cout << std::endl;
    }
  }
  
  // Step 5: Resolve addresses from our own functions
  std::cout << "Step 5: Resolving addresses from this program..." << std::endl;
  
  void* func1_addr = reinterpret_cast<void*>(&example_function_level_1);
  void* func2_addr = reinterpret_cast<void*>(&example_function_level_2);
  void* func3_addr = reinterpret_cast<void*>(&example_function_level_3);
  
  CallStack<> local_stack;
  local_stack.push(reinterpret_cast<uintptr_t>(func1_addr));
  local_stack.push(reinterpret_cast<uintptr_t>(func2_addr));
  local_stack.push(reinterpret_cast<uintptr_t>(func3_addr));
  
  auto resolved_local = converter.convert(local_stack, 0, true);
  
  std::cout << "  Call stack with " << resolved_local.size() << " frames:" << std::endl;
  for (size_t i = 0; i < resolved_local.size(); ++i) {
    print_resolved_frame(resolved_local[i], i);
    std::cout << std::endl;
  }
  
  // Step 6: Check cache statistics
  std::cout << "Step 6: Symbol resolver cache statistics" << std::endl;
  auto stats = resolver->get_cache_stats();
  std::cout << "  Cache hits: " << stats.hits << std::endl;
  std::cout << "  Cache misses: " << stats.misses << std::endl;
  std::cout << "  Cache size: " << stats.size << " entries" << std::endl;
  
  if (stats.hits + stats.misses > 0) {
    double hit_rate = (100.0 * stats.hits) / (stats.hits + stats.misses);
    std::cout << "  Hit rate: " << std::fixed << std::setprecision(1) 
              << hit_rate << "%" << std::endl;
  }
  std::cout << std::endl;
  
  // Step 7: Demonstrate different resolution strategies
  std::cout << "Step 7: Comparing different resolution strategies" << std::endl;
  std::cout << std::endl;
  
  if (printf_addr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(printf_addr);
    CallStack<> test_stack;
    test_stack.push(addr);
    
    // Try dladdr only
    std::cout << "  Strategy: DlAddr only (fast, export symbols)" << std::endl;
    auto resolver_dladdr = std::make_shared<SymbolResolver>(
        SymbolResolver::Strategy::kDlAddrOnly, false);
    OffsetConverter converter_dladdr(resolver_dladdr);
    converter_dladdr.add_map_snapshot(0, lib_map);
    auto resolved_dladdr = converter_dladdr.convert(test_stack, 0, true);
    print_resolved_frame(resolved_dladdr[0], 0);
    std::cout << std::endl;
    
    // Try addr2line only (may be slower but more comprehensive)
    std::cout << "  Strategy: Addr2line only (slower, DWARF debug info)" << std::endl;
    auto resolver_addr2line = std::make_shared<SymbolResolver>(
        SymbolResolver::Strategy::kAddr2LineOnly, false);
    OffsetConverter converter_addr2line(resolver_addr2line);
    converter_addr2line.add_map_snapshot(0, lib_map);
    auto resolved_addr2line = converter_addr2line.convert(test_stack, 0, true);
    print_resolved_frame(resolved_addr2line[0], 0);
    std::cout << std::endl;
  }
  
  std::cout << "=== Example Complete ===" << std::endl;
  std::cout << std::endl;
  std::cout << "Key Features Demonstrated:" << std::endl;
  std::cout << "  - Symbol resolution is optional and backward compatible" << std::endl;
  std::cout << "  - Multiple resolution strategies available (dladdr, addr2line, auto-fallback)" << std::endl;
  std::cout << "  - Symbol caching for performance" << std::endl;
  std::cout << "  - Resolves function names and source locations when available" << std::endl;
  std::cout << "  - Graceful degradation when symbols not available" << std::endl;
  
  return 0;
}
