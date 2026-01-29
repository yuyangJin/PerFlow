// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "sampling/library_map.h"

using namespace perflow::sampling;

class LibraryMapTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
  
  // Helper function to create a test maps file
  void CreateTestMapsFile(const char* filepath) {
    FILE* f = std::fopen(filepath, "w");
    ASSERT_NE(f, nullptr);
    
    // Write some example lines from /proc/self/maps
    std::fprintf(f, "400000-401000 r-xp 00000000 08:01 12345 /usr/bin/test\n");
    std::fprintf(f, "401000-402000 r--p 00001000 08:01 12345 /usr/bin/test\n");
    std::fprintf(f, "7f8000000000-7f8000021000 r-xp 00000000 08:01 67890 /lib/x86_64-linux-gnu/libc.so.6\n");
    std::fprintf(f, "7f8000021000-7f8000221000 ---p 00021000 08:01 67890 /lib/x86_64-linux-gnu/libc.so.6\n");
    std::fprintf(f, "7f8000221000-7f8000225000 r--p 00021000 08:01 67890 /lib/x86_64-linux-gnu/libc.so.6\n");
    std::fprintf(f, "7f8000225000-7f8000227000 rw-p 00025000 08:01 67890 /lib/x86_64-linux-gnu/libc.so.6\n");
    
    std::fclose(f);
  }
};

TEST_F(LibraryMapTest, DefaultConstruction) {
  LibraryMap lib_map;
  EXPECT_TRUE(lib_map.empty());
  EXPECT_EQ(lib_map.size(), 0u);
}

TEST_F(LibraryMapTest, ParseMapsFile) {
  LibraryMap lib_map;
  
  // Create a temporary test file
  const char* test_file = "/tmp/test_maps_file.txt";
  CreateTestMapsFile(test_file);
  
  // Parse the file
  ASSERT_TRUE(lib_map.parse_maps_file(test_file));
  
  // Should have parsed 2 executable regions
  EXPECT_EQ(lib_map.size(), 2u);
  EXPECT_FALSE(lib_map.empty());
  
  // Clean up
  std::remove(test_file);
}

TEST_F(LibraryMapTest, ResolveAddress) {
  LibraryMap lib_map;
  
  // Create a temporary test file
  const char* test_file = "/tmp/test_maps_resolve.txt";
  CreateTestMapsFile(test_file);
  
  // Parse the file
  ASSERT_TRUE(lib_map.parse_maps_file(test_file));
  
  // Resolve an address in the first region
  auto result1 = lib_map.resolve(0x400500);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(result1->first, "/usr/bin/test");
  EXPECT_EQ(result1->second, 0x500u);
  
  // Resolve an address in the second region
  auto result2 = lib_map.resolve(0x7f8000010000);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->first, "/lib/x86_64-linux-gnu/libc.so.6");
  EXPECT_EQ(result2->second, 0x10000u);
  
  // Try to resolve an address not in any region
  auto result3 = lib_map.resolve(0x12345678);
  EXPECT_FALSE(result3.has_value());
  
  // Clean up
  std::remove(test_file);
}

TEST_F(LibraryMapTest, AddLibrary) {
  LibraryMap lib_map;
  
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x1000, 0x2000, true);
  LibraryMap::LibraryInfo lib2("/test/lib2.so", 0x3000, 0x4000, true);
  
  lib_map.add_library(lib1);
  lib_map.add_library(lib2);
  
  EXPECT_EQ(lib_map.size(), 2u);
  
  // Test resolution
  auto result = lib_map.resolve(0x1500);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "/test/lib1.so");
  EXPECT_EQ(result->second, 0x500u);
}

TEST_F(LibraryMapTest, Clear) {
  LibraryMap lib_map;
  
  LibraryMap::LibraryInfo lib1("/test/lib.so", 0x1000, 0x2000, true);
  lib_map.add_library(lib1);
  
  EXPECT_FALSE(lib_map.empty());
  
  lib_map.clear();
  
  EXPECT_TRUE(lib_map.empty());
  EXPECT_EQ(lib_map.size(), 0u);
}

TEST_F(LibraryMapTest, ParseCurrentProcess) {
  LibraryMap lib_map;
  
  // Parse the actual /proc/self/maps
  ASSERT_TRUE(lib_map.parse_current_process());
  
  // Should have found at least a few libraries
  EXPECT_GT(lib_map.size(), 0u);
  
  // Check that at least one library has been loaded
  const auto& libraries = lib_map.libraries();
  ASSERT_GT(libraries.size(), 0u);
  
  // Verify that we can resolve an address within the first library
  if (libraries.size() > 0) {
    const auto& first_lib = libraries[0];
    uintptr_t test_addr = first_lib.base + 0x100;  // An address within the first library
    
    auto result = lib_map.resolve(test_addr);
    EXPECT_TRUE(result.has_value());
    if (result.has_value()) {
      EXPECT_FALSE(result->first.empty());
      EXPECT_EQ(result->second, 0x100u);
    }
  }
}

TEST_F(LibraryMapTest, LibraryInfoConstruction) {
  LibraryMap::LibraryInfo lib1;
  EXPECT_EQ(lib1.name, "");
  EXPECT_EQ(lib1.base, 0u);
  EXPECT_EQ(lib1.end, 0u);
  EXPECT_FALSE(lib1.executable);
  
  LibraryMap::LibraryInfo lib2("/test/lib.so", 0x1000, 0x2000, true);
  EXPECT_EQ(lib2.name, "/test/lib.so");
  EXPECT_EQ(lib2.base, 0x1000u);
  EXPECT_EQ(lib2.end, 0x2000u);
  EXPECT_TRUE(lib2.executable);
}
