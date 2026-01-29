// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "analysis/offset_converter.h"
#include "sampling/call_stack.h"
#include "sampling/library_map.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

class OffsetConverterTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OffsetConverterTest, DefaultConstruction) {
  OffsetConverter converter;
  EXPECT_EQ(converter.snapshot_count(), 0u);
  EXPECT_FALSE(converter.has_snapshot(0));
}

TEST_F(OffsetConverterTest, AddMapSnapshot) {
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add some libraries
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x1000, 0x2000, true);
  lib_map.add_library(lib1);
  
  converter.add_map_snapshot(0, lib_map);
  
  EXPECT_EQ(converter.snapshot_count(), 1u);
  EXPECT_TRUE(converter.has_snapshot(0));
  EXPECT_FALSE(converter.has_snapshot(1));
}

TEST_F(OffsetConverterTest, ConvertCallStackWithSnapshot) {
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add some libraries
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x1000, 0x2000, true);
  LibraryMap::LibraryInfo lib2("/test/lib2.so", 0x3000, 0x4000, true);
  lib_map.add_library(lib1);
  lib_map.add_library(lib2);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack
  uintptr_t addresses[] = {0x1500, 0x3500, 0x1800};
  CallStack<> stack(addresses, 3);
  
  // Convert the stack
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 3u);
  
  // Check first frame
  EXPECT_EQ(resolved[0].raw_address, 0x1500u);
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[0].offset, 0x500u);
  
  // Check second frame
  EXPECT_EQ(resolved[1].raw_address, 0x3500u);
  EXPECT_EQ(resolved[1].library_name, "/test/lib2.so");
  EXPECT_EQ(resolved[1].offset, 0x500u);
  
  // Check third frame
  EXPECT_EQ(resolved[2].raw_address, 0x1800u);
  EXPECT_EQ(resolved[2].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[2].offset, 0x800u);
}

TEST_F(OffsetConverterTest, ConvertCallStackWithoutSnapshot) {
  OffsetConverter converter;
  
  // Create a call stack
  uintptr_t addresses[] = {0x1500, 0x3500};
  CallStack<> stack(addresses, 2);
  
  // Convert without adding a snapshot
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 2u);
  
  // Should have raw addresses but no resolution
  EXPECT_EQ(resolved[0].raw_address, 0x1500u);
  EXPECT_EQ(resolved[0].library_name, "[unknown]");
  EXPECT_EQ(resolved[0].offset, 0x1500u);
  
  EXPECT_EQ(resolved[1].raw_address, 0x3500u);
  EXPECT_EQ(resolved[1].library_name, "[unknown]");
  EXPECT_EQ(resolved[1].offset, 0x3500u);
}

TEST_F(OffsetConverterTest, ConvertUnresolvedAddress) {
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add one library
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x1000, 0x2000, true);
  lib_map.add_library(lib1);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack with an address outside known regions
  uintptr_t addresses[] = {0x1500, 0x9999};
  CallStack<> stack(addresses, 2);
  
  // Convert the stack
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 2u);
  
  // First should resolve
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  
  // Second should not resolve
  EXPECT_EQ(resolved[1].raw_address, 0x9999u);
  EXPECT_EQ(resolved[1].library_name, "[unresolved]");
  EXPECT_EQ(resolved[1].offset, 0x9999u);
}

TEST_F(OffsetConverterTest, MultipleSnapshots) {
  OffsetConverter converter;
  
  // Create two different library maps
  LibraryMap lib_map1;
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x1000, 0x2000, true);
  lib_map1.add_library(lib1);
  
  LibraryMap lib_map2;
  LibraryMap::LibraryInfo lib2("/test/lib2.so", 0x3000, 0x4000, true);
  lib_map2.add_library(lib2);
  
  converter.add_map_snapshot(0, lib_map1);
  converter.add_map_snapshot(1, lib_map2);
  
  EXPECT_EQ(converter.snapshot_count(), 2u);
  EXPECT_TRUE(converter.has_snapshot(0));
  EXPECT_TRUE(converter.has_snapshot(1));
  
  // Create call stacks
  uintptr_t addresses1[] = {0x1500};
  CallStack<> stack1(addresses1, 1);
  
  uintptr_t addresses2[] = {0x3500};
  CallStack<> stack2(addresses2, 1);
  
  // Convert with different snapshots
  auto resolved1 = converter.convert(stack1, 0);
  auto resolved2 = converter.convert(stack2, 1);
  
  EXPECT_EQ(resolved1[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved2[0].library_name, "/test/lib2.so");
}

TEST_F(OffsetConverterTest, Clear) {
  OffsetConverter converter;
  LibraryMap lib_map;
  
  converter.add_map_snapshot(0, lib_map);
  EXPECT_EQ(converter.snapshot_count(), 1u);
  
  converter.clear();
  EXPECT_EQ(converter.snapshot_count(), 0u);
  EXPECT_FALSE(converter.has_snapshot(0));
}

TEST_F(OffsetConverterTest, EmptyCallStack) {
  OffsetConverter converter;
  LibraryMap lib_map;
  converter.add_map_snapshot(0, lib_map);
  
  CallStack<> empty_stack;
  auto resolved = converter.convert(empty_stack, 0);
  
  EXPECT_EQ(resolved.size(), 0u);
}
