// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <memory>

#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"
#include "sampling/call_stack.h"
#include "sampling/library_map.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

class OffsetConverterSymbolResolutionTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OffsetConverterSymbolResolutionTest, ConverterWithoutSymbolResolver) {
  // Test backward compatibility - converter without symbol resolver
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add a library
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x7f0000001000, 0x7f0000002000, true);
  lib_map.add_library(lib1);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack
  uintptr_t addresses[] = {0x7f0000001500};
  CallStack<> stack(addresses, 1);
  
  // Convert without symbol resolution (default behavior)
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 1u);
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[0].offset, 0x500u);
  
  // Function name should be hex offset when no resolver (not empty)
  EXPECT_FALSE(resolved[0].function_name.empty());
  EXPECT_EQ(resolved[0].function_name, "0x500");
  EXPECT_TRUE(resolved[0].filename.empty());
  EXPECT_EQ(resolved[0].line_number, 0u);
}

TEST_F(OffsetConverterSymbolResolutionTest, ConverterWithSymbolResolver) {
  // Create symbol resolver
  auto resolver = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kAutoFallback, true);
  
  // Create converter with symbol resolver
  OffsetConverter converter(resolver);
  
  EXPECT_TRUE(converter.has_symbol_resolver());
  
  LibraryMap lib_map;
  LibraryMap::LibraryInfo lib1("/lib/x86_64-linux-gnu/libc.so.6", 0x7f8000000000, 0x7f8000200000, true);
  lib_map.add_library(lib1);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack
  uintptr_t addresses[] = {0x7f8000000500};
  CallStack<> stack(addresses, 1);
  
  // Convert WITH symbol resolution
  auto resolved = converter.convert(stack, 0, true);
  
  ASSERT_EQ(resolved.size(), 1u);
  EXPECT_EQ(resolved[0].library_name, "/lib/x86_64-linux-gnu/libc.so.6");
  EXPECT_EQ(resolved[0].offset, 0x500u);
  
  // Symbol fields may or may not be populated depending on whether
  // the address actually resolves to a symbol, but the call shouldn't crash
}

TEST_F(OffsetConverterSymbolResolutionTest, SetSymbolResolverAfterConstruction) {
  OffsetConverter converter;
  
  EXPECT_FALSE(converter.has_symbol_resolver());
  
  auto resolver = std::make_shared<SymbolResolver>();
  converter.set_symbol_resolver(resolver);
  
  EXPECT_TRUE(converter.has_symbol_resolver());
}

TEST_F(OffsetConverterSymbolResolutionTest, DisableSymbolResolver) {
  auto resolver = std::make_shared<SymbolResolver>();
  OffsetConverter converter(resolver);
  
  EXPECT_TRUE(converter.has_symbol_resolver());
  
  // Disable by setting nullptr
  converter.set_symbol_resolver(nullptr);
  
  EXPECT_FALSE(converter.has_symbol_resolver());
}

TEST_F(OffsetConverterSymbolResolutionTest, MultipleFramesWithSymbolResolution) {
  auto resolver = std::make_shared<SymbolResolver>();
  OffsetConverter converter(resolver);
  
  LibraryMap lib_map;
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x7f0000001000, 0x7f0000002000, true);
  LibraryMap::LibraryInfo lib2("/test/lib2.so", 0x7f0000003000, 0x7f0000004000, true);
  lib_map.add_library(lib1);
  lib_map.add_library(lib2);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a multi-frame call stack
  uintptr_t addresses[] = {0x7f0000001500, 0x7f0000003500, 0x7f0000001800};
  CallStack<> stack(addresses, 3);
  
  // Convert with symbol resolution
  auto resolved = converter.convert(stack, 0, true);
  
  ASSERT_EQ(resolved.size(), 3u);
  
  // Verify library resolution worked
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[1].library_name, "/test/lib2.so");
  EXPECT_EQ(resolved[2].library_name, "/test/lib1.so");
  
  // Verify offsets
  EXPECT_EQ(resolved[0].offset, 0x500u);
  EXPECT_EQ(resolved[1].offset, 0x500u);
  EXPECT_EQ(resolved[2].offset, 0x800u);
}

TEST_F(OffsetConverterSymbolResolutionTest, SymbolResolutionRequestedButNoResolver) {
  // Create converter without resolver
  OffsetConverter converter;
  
  LibraryMap lib_map;
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x7f0000001000, 0x7f0000002000, true);
  lib_map.add_library(lib1);
  
  converter.add_map_snapshot(0, lib_map);
  
  uintptr_t addresses[] = {0x7f0000001500};
  CallStack<> stack(addresses, 1);
  
  // Request symbol resolution but no resolver is set
  // Should not crash, just use hex offset as function name
  auto resolved = converter.convert(stack, 0, true);
  
  ASSERT_EQ(resolved.size(), 1u);
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[0].function_name, "0x500");
}

TEST_F(OffsetConverterSymbolResolutionTest, BackwardCompatibilityWithExistingCode) {
  // Existing code that doesn't know about symbol resolution
  OffsetConverter converter;
  LibraryMap lib_map;
  
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x7f0000001000, 0x7f0000002000, true);
  lib_map.add_library(lib1);
  converter.add_map_snapshot(0, lib_map);
  
  uintptr_t addresses[] = {0x7f0000001500};
  CallStack<> stack(addresses, 1);
  
  // Old-style call (2 parameters) should still work
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 1u);
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[0].offset, 0x500u);
}

TEST_F(OffsetConverterSymbolResolutionTest, UnresolvedAddressWithSymbolResolver) {
  auto resolver = std::make_shared<SymbolResolver>();
  OffsetConverter converter(resolver);
  
  LibraryMap lib_map;
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x7f0000001000, 0x7f0000002000, true);
  lib_map.add_library(lib1);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Address outside known libraries
  uintptr_t addresses[] = {0x9999};
  CallStack<> stack(addresses, 1);
  
  auto resolved = converter.convert(stack, 0, true);
  
  ASSERT_EQ(resolved.size(), 1u);
  EXPECT_EQ(resolved[0].library_name, "[unresolved]");
  EXPECT_EQ(resolved[0].offset, 0x9999u);
  // For unresolved addresses, function_name is the hex address
  EXPECT_EQ(resolved[0].function_name, "0x9999");
}

TEST_F(OffsetConverterSymbolResolutionTest, SymbolCachingAcrossMultipleCalls) {
  auto resolver = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kAutoFallback, true);
  OffsetConverter converter(resolver);
  
  LibraryMap lib_map;
  LibraryMap::LibraryInfo lib1("/test/lib.so", 0x7f0000001000, 0x7f0000002000, true);
  lib_map.add_library(lib1);
  converter.add_map_snapshot(0, lib_map);
  
  // Create multiple stacks with same addresses
  uintptr_t addresses[] = {0x7f0000001500};
  CallStack<> stack1(addresses, 1);
  CallStack<> stack2(addresses, 1);
  
  // First conversion
  converter.convert(stack1, 0, true);
  auto stats_after_first = resolver->get_cache_stats();
  
  // Second conversion (should use cache)
  converter.convert(stack2, 0, true);
  auto stats_after_second = resolver->get_cache_stats();
  
  // Cache should have been used
  EXPECT_GT(stats_after_second.hits, stats_after_first.hits);
}

TEST_F(OffsetConverterSymbolResolutionTest, DifferentStrategies) {
  // Test with different resolution strategies
  
  // DlAddr only
  auto resolver1 = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kDlAddrOnly);
  OffsetConverter converter1(resolver1);
  EXPECT_TRUE(converter1.has_symbol_resolver());
  
  // Addr2line only
  auto resolver2 = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kAddr2LineOnly);
  OffsetConverter converter2(resolver2);
  EXPECT_TRUE(converter2.has_symbol_resolver());
  
  // Auto fallback
  auto resolver3 = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kAutoFallback);
  OffsetConverter converter3(resolver3);
  EXPECT_TRUE(converter3.has_symbol_resolver());
}

TEST_F(OffsetConverterSymbolResolutionTest, EmptyStackWithSymbolResolver) {
  auto resolver = std::make_shared<SymbolResolver>();
  OffsetConverter converter(resolver);
  
  LibraryMap lib_map;
  converter.add_map_snapshot(0, lib_map);
  
  CallStack<> empty_stack;
  auto resolved = converter.convert(empty_stack, 0, true);
  
  EXPECT_EQ(resolved.size(), 0u);
}
