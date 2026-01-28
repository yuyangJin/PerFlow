// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

#include "analysis/offset_converter.h"
#include "sampling/call_stack.h"
#include "sampling/library_map.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

class OffsetConverterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test maps files
    std::string map1_content = 
      "7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/x86_64-linux-gnu/libc.so.6\n"
      "7ffff7dd5000-7ffff7dfc000 r-xp 00000000 08:01 2345678 /lib64/ld-linux-x86-64.so.2\n";
    
    std::string map2_content = 
      "7f8a4d000000-7f8a4d100000 r-xp 00000000 08:01 3456789 /usr/lib/libm.so.6\n"
      "7f8a4e000000-7f8a4e050000 r-xp 00000000 08:01 4567890 /usr/lib/libpthread.so.0\n";
    
    test_map1_file_ = createTestMapsFile(map1_content);
    test_map2_file_ = createTestMapsFile(map2_content);
  }

  void TearDown() override {
    cleanupTestFile(test_map1_file_);
    cleanupTestFile(test_map2_file_);
  }

  std::string createTestMapsFile(const std::string& content) {
    char temp_filename[] = "/tmp/test_maps_XXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd == -1) {
      return "";
    }
    
    std::string filename(temp_filename);
    close(fd);
    
    std::ofstream file(filename);
    file << content;
    file.close();
    
    return filename;
  }

  void cleanupTestFile(const std::string& filename) {
    if (!filename.empty()) {
      std::remove(filename.c_str());
    }
  }

  std::string test_map1_file_;
  std::string test_map2_file_;
};

TEST_F(OffsetConverterTest, AddAndRetrieveSnapshot) {
  OffsetConverter converter;
  
  LibraryMap map1;
  ASSERT_TRUE(map1.parse_maps_file(test_map1_file_));
  
  converter.add_map_snapshot(0, map1);
  
  EXPECT_EQ(converter.snapshot_count(), 1u);
  EXPECT_TRUE(converter.has_snapshot(0));
  EXPECT_FALSE(converter.has_snapshot(1));
  
  const LibraryMap* retrieved = converter.get_snapshot(0);
  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved->size(), map1.size());
}

TEST_F(OffsetConverterTest, ConvertSimpleCallStack) {
  OffsetConverter converter;
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map);
  
  // Create a raw call stack with addresses in libc
  uintptr_t addresses[] = {
    0x7f8a4c010000,  // libc base + 0x10000
    0x7f8a4c015000,  // libc base + 0x15000
    0x7ffff7dd6000   // ld base + 0x1000
  };
  
  CallStack<> stack(addresses, 3);
  RawCallStack<> raw(stack, 1234567890, 0);
  
  ResolvedCallStack resolved = converter.convert(raw);
  
  EXPECT_EQ(resolved.timestamp, 1234567890);
  EXPECT_EQ(resolved.map_id, 0u);
  ASSERT_EQ(resolved.frames.size(), 3u);
  
  // Check first frame
  EXPECT_EQ(resolved.frames[0].library_name, "/lib/x86_64-linux-gnu/libc.so.6");
  EXPECT_EQ(resolved.frames[0].offset, 0x10000u);
  EXPECT_EQ(resolved.frames[0].raw_address, 0x7f8a4c010000u);
  
  // Check second frame
  EXPECT_EQ(resolved.frames[1].library_name, "/lib/x86_64-linux-gnu/libc.so.6");
  EXPECT_EQ(resolved.frames[1].offset, 0x15000u);
  
  // Check third frame
  EXPECT_EQ(resolved.frames[2].library_name, "/lib64/ld-linux-x86-64.so.2");
  EXPECT_EQ(resolved.frames[2].offset, 0x1000u);
}

TEST_F(OffsetConverterTest, ConvertWithUnknownAddress) {
  OffsetConverter converter;
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map);
  
  // Create call stack with one valid and one invalid address
  uintptr_t addresses[] = {
    0x7f8a4c010000,  // Valid: in libc
    0x1000           // Invalid: not in any library
  };
  
  CallStack<> stack(addresses, 2);
  RawCallStack<> raw(stack, 0, 0);
  
  ResolvedCallStack resolved = converter.convert(raw);
  
  ASSERT_EQ(resolved.frames.size(), 2u);
  
  // First frame should be resolved
  EXPECT_EQ(resolved.frames[0].library_name, "/lib/x86_64-linux-gnu/libc.so.6");
  EXPECT_EQ(resolved.frames[0].offset, 0x10000u);
  
  // Second frame should be unknown
  EXPECT_EQ(resolved.frames[1].library_name, "[unknown]");
  EXPECT_EQ(resolved.frames[1].offset, 0x1000u);  // Stored as-is
  EXPECT_EQ(resolved.frames[1].raw_address, 0x1000u);
}

TEST_F(OffsetConverterTest, ConvertWithMissingMapId) {
  OffsetConverter converter;
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map);
  
  // Create raw stack with map_id that doesn't exist
  uintptr_t addresses[] = {0x7f8a4c010000};
  CallStack<> stack(addresses, 1);
  RawCallStack<> raw(stack, 0, 999);  // Non-existent map_id
  
  ResolvedCallStack resolved = converter.convert(raw);
  
  ASSERT_EQ(resolved.frames.size(), 1u);
  EXPECT_EQ(resolved.frames[0].library_name, "[unknown]");
}

TEST_F(OffsetConverterTest, ConvertBatch) {
  OffsetConverter converter;
  
  LibraryMap map1;
  ASSERT_TRUE(map1.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map1);
  
  LibraryMap map2;
  ASSERT_TRUE(map2.parse_maps_file(test_map2_file_));
  converter.add_map_snapshot(1, map2);
  
  // Create batch of raw stacks
  std::vector<RawCallStack<>> raw_stacks;
  
  uintptr_t addr1[] = {0x7f8a4c010000};  // Map 0 (libc)
  CallStack<> stack1(addr1, 1);
  raw_stacks.emplace_back(stack1, 100, 0);
  
  uintptr_t addr2[] = {0x7f8a4d020000};  // Map 1 (libm)
  CallStack<> stack2(addr2, 1);
  raw_stacks.emplace_back(stack2, 200, 1);
  
  auto resolved_stacks = converter.convert_batch(raw_stacks);
  
  ASSERT_EQ(resolved_stacks.size(), 2u);
  
  EXPECT_EQ(resolved_stacks[0].timestamp, 100);
  EXPECT_EQ(resolved_stacks[0].map_id, 0u);
  EXPECT_EQ(resolved_stacks[0].frames[0].library_name, "/lib/x86_64-linux-gnu/libc.so.6");
  
  EXPECT_EQ(resolved_stacks[1].timestamp, 200);
  EXPECT_EQ(resolved_stacks[1].map_id, 1u);
  EXPECT_EQ(resolved_stacks[1].frames[0].library_name, "/usr/lib/libm.so.6");
  EXPECT_EQ(resolved_stacks[1].frames[0].offset, 0x20000u);
}

TEST_F(OffsetConverterTest, ConvertWithMapId) {
  OffsetConverter converter;
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map);
  
  uintptr_t addresses[] = {0x7f8a4c010000};
  CallStack<> stack(addresses, 1);
  
  ResolvedCallStack resolved = converter.convert_with_map_id(stack, 0, 999);
  
  EXPECT_EQ(resolved.timestamp, 999);
  EXPECT_EQ(resolved.map_id, 0u);
  ASSERT_EQ(resolved.frames.size(), 1u);
  EXPECT_EQ(resolved.frames[0].library_name, "/lib/x86_64-linux-gnu/libc.so.6");
}

TEST_F(OffsetConverterTest, MultipleSnapshots) {
  OffsetConverter converter;
  
  LibraryMap map1;
  ASSERT_TRUE(map1.parse_maps_file(test_map1_file_));
  
  LibraryMap map2;
  ASSERT_TRUE(map2.parse_maps_file(test_map2_file_));
  
  converter.add_map_snapshot(0, map1);
  converter.add_map_snapshot(1, map2);
  
  EXPECT_EQ(converter.snapshot_count(), 2u);
  EXPECT_TRUE(converter.has_snapshot(0));
  EXPECT_TRUE(converter.has_snapshot(1));
  EXPECT_FALSE(converter.has_snapshot(2));
}

TEST_F(OffsetConverterTest, ClearSnapshots) {
  OffsetConverter converter;
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map);
  
  EXPECT_EQ(converter.snapshot_count(), 1u);
  
  converter.clear();
  
  EXPECT_EQ(converter.snapshot_count(), 0u);
  EXPECT_FALSE(converter.has_snapshot(0));
}

TEST_F(OffsetConverterTest, EmptyCallStack) {
  OffsetConverter converter;
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(test_map1_file_));
  converter.add_map_snapshot(0, map);
  
  CallStack<> empty_stack;
  RawCallStack<> raw(empty_stack, 0, 0);
  
  ResolvedCallStack resolved = converter.convert(raw);
  
  EXPECT_EQ(resolved.frames.size(), 0u);
}

TEST_F(OffsetConverterTest, RawCallStackConstructors) {
  CallStack<> stack;
  stack.push(0x1000);
  
  RawCallStack<> raw1;
  EXPECT_EQ(raw1.timestamp, 0);
  EXPECT_EQ(raw1.map_id, 0u);
  
  RawCallStack<> raw2(stack, 123, 456);
  EXPECT_EQ(raw2.timestamp, 123);
  EXPECT_EQ(raw2.map_id, 456u);
  EXPECT_EQ(raw2.addresses.depth(), 1u);
  
  RawCallStack<> raw3(raw2);
  EXPECT_EQ(raw3.timestamp, 123);
  EXPECT_EQ(raw3.map_id, 456u);
  
  RawCallStack<> raw4;
  raw4 = raw2;
  EXPECT_EQ(raw4.timestamp, 123);
  EXPECT_EQ(raw4.map_id, 456u);
}

TEST_F(OffsetConverterTest, ResolvedFrameConstructors) {
  ResolvedFrame frame1;
  EXPECT_TRUE(frame1.library_name.empty());
  EXPECT_EQ(frame1.offset, 0u);
  EXPECT_EQ(frame1.raw_address, 0u);
  
  ResolvedFrame frame2("/lib/test.so", 0x1000, 0x7fff1000);
  EXPECT_EQ(frame2.library_name, "/lib/test.so");
  EXPECT_EQ(frame2.offset, 0x1000u);
  EXPECT_EQ(frame2.raw_address, 0x7fff1000u);
}

TEST_F(OffsetConverterTest, ResolvedCallStackConstructors) {
  ResolvedCallStack resolved1;
  EXPECT_TRUE(resolved1.frames.empty());
  EXPECT_EQ(resolved1.timestamp, 0);
  EXPECT_EQ(resolved1.map_id, 0u);
  
  ResolvedCallStack resolved2(123, 456);
  EXPECT_EQ(resolved2.timestamp, 123);
  EXPECT_EQ(resolved2.map_id, 456u);
}
