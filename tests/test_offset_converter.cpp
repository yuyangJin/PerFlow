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
  
  // Add some libraries with realistic dynamic base addresses
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x7f0000001000, 0x7f0000002000, true);
  LibraryMap::LibraryInfo lib2("/test/lib2.so", 0x7f0000003000, 0x7f0000004000, true);
  lib_map.add_library(lib1);
  lib_map.add_library(lib2);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack
  uintptr_t addresses[] = {0x7f0000001500, 0x7f0000003500, 0x7f0000001800};
  CallStack<> stack(addresses, 3);
  
  // Convert the stack
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 3u);
  
  // Check first frame
  EXPECT_EQ(resolved[0].raw_address, 0x7f0000001500u);
  EXPECT_EQ(resolved[0].library_name, "/test/lib1.so");
  EXPECT_EQ(resolved[0].offset, 0x500u);
  
  // Check second frame
  EXPECT_EQ(resolved[1].raw_address, 0x7f0000003500u);
  EXPECT_EQ(resolved[1].library_name, "/test/lib2.so");
  EXPECT_EQ(resolved[1].offset, 0x500u);
  
  // Check third frame
  EXPECT_EQ(resolved[2].raw_address, 0x7f0000001800u);
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

TEST_F(OffsetConverterTest, StaticBaseAddressMPIProgram) {
  // Test for MPI programs with static base addresses
  // For static base addresses (< 0x10000000), the offset should equal the raw address
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add a library with a static base address (typical for non-PIE executables)
  LibraryMap::LibraryInfo mpi_program("/path/to/mpi_program", 0x402000, 0x500000, true);
  lib_map.add_library(mpi_program);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack with addresses in the static range
  uintptr_t addresses[] = {0x410000, 0x420000, 0x450000};
  CallStack<> stack(addresses, 3);
  
  // Convert the stack
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 3u);
  
  // For static base addresses, offset should equal raw address
  EXPECT_EQ(resolved[0].raw_address, 0x410000u);
  EXPECT_EQ(resolved[0].library_name, "/path/to/mpi_program");
  EXPECT_EQ(resolved[0].offset, 0x410000u);  // NOT 0x410000 - 0x402000 = 0xe000
  
  EXPECT_EQ(resolved[1].raw_address, 0x420000u);
  EXPECT_EQ(resolved[1].library_name, "/path/to/mpi_program");
  EXPECT_EQ(resolved[1].offset, 0x420000u);  // NOT 0x420000 - 0x402000 = 0x1e000
  
  EXPECT_EQ(resolved[2].raw_address, 0x450000u);
  EXPECT_EQ(resolved[2].library_name, "/path/to/mpi_program");
  EXPECT_EQ(resolved[2].offset, 0x450000u);  // NOT 0x450000 - 0x402000 = 0x4e000
}

TEST_F(OffsetConverterTest, DynamicBaseAddressSharedLibrary) {
  // Test for shared libraries with dynamic base addresses
  // For dynamic base addresses (>= 0x10000000), the offset should be calculated
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add a library with a dynamic base address (typical for shared libraries with ASLR)
  LibraryMap::LibraryInfo shared_lib("/lib/libc.so.6", 0x7f8a1c000000, 0x7f8a1c200000, true);
  lib_map.add_library(shared_lib);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack with addresses in the dynamic range
  uintptr_t addresses[] = {0x7f8a1c010000, 0x7f8a1c020000};
  CallStack<> stack(addresses, 2);
  
  // Convert the stack
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 2u);
  
  // For dynamic base addresses, offset should be calculated as raw_address - base
  EXPECT_EQ(resolved[0].raw_address, 0x7f8a1c010000u);
  EXPECT_EQ(resolved[0].library_name, "/lib/libc.so.6");
  EXPECT_EQ(resolved[0].offset, 0x10000u);  // 0x7f8a1c010000 - 0x7f8a1c000000
  
  EXPECT_EQ(resolved[1].raw_address, 0x7f8a1c020000u);
  EXPECT_EQ(resolved[1].library_name, "/lib/libc.so.6");
  EXPECT_EQ(resolved[1].offset, 0x20000u);  // 0x7f8a1c020000 - 0x7f8a1c000000
}

TEST_F(OffsetConverterTest, MixedStaticAndDynamicAddresses) {
  // Test with both static and dynamic addresses in the same snapshot
  OffsetConverter converter;
  LibraryMap lib_map;
  
  // Add both static and dynamic libraries
  LibraryMap::LibraryInfo mpi_program("/path/to/mpi_program", 0x402000, 0x500000, true);
  LibraryMap::LibraryInfo shared_lib("/lib/libc.so.6", 0x7f8a1c000000, 0x7f8a1c200000, true);
  lib_map.add_library(mpi_program);
  lib_map.add_library(shared_lib);
  
  converter.add_map_snapshot(0, lib_map);
  
  // Create a call stack with both static and dynamic addresses
  uintptr_t addresses[] = {0x410000, 0x7f8a1c010000, 0x420000};
  CallStack<> stack(addresses, 3);
  
  // Convert the stack
  auto resolved = converter.convert(stack, 0);
  
  ASSERT_EQ(resolved.size(), 3u);
  
  // Static address - offset equals raw address
  EXPECT_EQ(resolved[0].raw_address, 0x410000u);
  EXPECT_EQ(resolved[0].library_name, "/path/to/mpi_program");
  EXPECT_EQ(resolved[0].offset, 0x410000u);
  
  // Dynamic address - offset is calculated
  EXPECT_EQ(resolved[1].raw_address, 0x7f8a1c010000u);
  EXPECT_EQ(resolved[1].library_name, "/lib/libc.so.6");
  EXPECT_EQ(resolved[1].offset, 0x10000u);
  
  // Static address - offset equals raw address
  EXPECT_EQ(resolved[2].raw_address, 0x420000u);
  EXPECT_EQ(resolved[2].library_name, "/path/to/mpi_program");
  EXPECT_EQ(resolved[2].offset, 0x420000u);
}
