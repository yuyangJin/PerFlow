// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <unistd.h>

#include "sampling/library_map.h"

using namespace perflow::sampling;

class LibraryMapTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}

  // Helper to create a test maps file
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
};

TEST_F(LibraryMapTest, ParseCurrentProcess) {
  LibraryMap map;
  EXPECT_TRUE(map.parse_current_process());
  EXPECT_GT(map.size(), 0u);
  EXPECT_FALSE(map.empty());
}

TEST_F(LibraryMapTest, ParseValidMapsFile) {
  std::string content = 
    "7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/x86_64-linux-gnu/libc.so.6\n"
    "7f8a4c021000-7f8a4c221000 ---p 00021000 08:01 1234567 /lib/x86_64-linux-gnu/libc.so.6\n"
    "7f8a4c221000-7f8a4c225000 r-xp 00221000 08:01 1234567 /lib/x86_64-linux-gnu/libc.so.6\n"
    "7ffff7dd5000-7ffff7dfc000 r-xp 00000000 08:01 2345678 /lib64/ld-linux-x86-64.so.2\n";
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  EXPECT_TRUE(map.parse_maps_file(filename));
  EXPECT_EQ(map.size(), 3u);  // Only executable regions
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, ResolveAddress) {
  std::string content = 
    "7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/x86_64-linux-gnu/libc.so.6\n"
    "7ffff7dd5000-7ffff7dfc000 r-xp 00000000 08:01 2345678 /lib64/ld-linux-x86-64.so.2\n";
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  ASSERT_TRUE(map.parse_maps_file(filename));
  
  // Test address within first library
  uintptr_t addr1 = 0x7f8a4c010000;
  auto result1 = map.resolve(addr1);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(result1->first, "/lib/x86_64-linux-gnu/libc.so.6");
  EXPECT_EQ(result1->second, 0x10000u);
  
  // Test address at start of second library
  uintptr_t addr2 = 0x7ffff7dd5000;
  auto result2 = map.resolve(addr2);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->first, "/lib64/ld-linux-x86-64.so.2");
  EXPECT_EQ(result2->second, 0u);
  
  // Test address outside any library
  uintptr_t addr3 = 0x1000;
  auto result3 = map.resolve(addr3);
  EXPECT_FALSE(result3.has_value());
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, ParseSpecialRegions) {
  std::string content = 
    "7ffff7dd5000-7ffff7dfc000 r-xp 00000000 08:01 2345678 /usr/bin/test_app\n"
    "7ffffffdd000-7ffffffff000 r-xp 00000000 00:00 0 [stack]\n"
    "ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0 [vsyscall]\n";
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  EXPECT_TRUE(map.parse_maps_file(filename));
  EXPECT_EQ(map.size(), 3u);
  
  const auto& libs = map.libraries();
  EXPECT_EQ(libs[0].name, "/usr/bin/test_app");
  EXPECT_EQ(libs[1].name, "[stack]");
  EXPECT_EQ(libs[2].name, "[vsyscall]");
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, IgnoreNonExecutableRegions) {
  std::string content = 
    "7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/libc.so.6\n"
    "7f8a4c021000-7f8a4c221000 rw-p 00021000 08:01 1234567 /lib/libc.so.6\n"
    "7f8a4c221000-7f8a4c225000 r--p 00221000 08:01 1234567 /lib/libc.so.6\n";
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  EXPECT_TRUE(map.parse_maps_file(filename));
  EXPECT_EQ(map.size(), 1u);  // Only the first r-xp region
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, ClearMap) {
  std::string content = 
    "7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/libc.so.6\n";
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  EXPECT_TRUE(map.parse_maps_file(filename));
  EXPECT_FALSE(map.empty());
  
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, ParseInvalidFile) {
  LibraryMap map;
  EXPECT_FALSE(map.parse_maps_file("/nonexistent/file"));
  EXPECT_TRUE(map.empty());
}

TEST_F(LibraryMapTest, ParseMalformedLines) {
  std::string content = 
    "invalid line\n"
    "7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/libc.so.6\n"
    "not-a-valid-address range\n";
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  EXPECT_TRUE(map.parse_maps_file(filename));  // Should succeed and skip invalid lines
  EXPECT_EQ(map.size(), 1u);  // Only one valid line
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, HandleLargeNumberOfLibraries) {
  std::string content;
  
  // Create 1000 library entries
  for (int i = 0; i < 1000; ++i) {
    uintptr_t base = 0x7f0000000000ULL + (i * 0x100000);
    uintptr_t end = base + 0x10000;
    char line[256];
    std::snprintf(line, sizeof(line), 
                  "%016lx-%016lx r-xp 00000000 08:01 %d /lib/lib%d.so\n",
                  base, end, 1000000 + i, i);
    content += line;
  }
  
  std::string filename = createTestMapsFile(content);
  ASSERT_FALSE(filename.empty());
  
  LibraryMap map;
  EXPECT_TRUE(map.parse_maps_file(filename));
  EXPECT_EQ(map.size(), 1000u);
  
  // Test resolution for first and last library
  auto result_first = map.resolve(0x7f0000000000ULL);
  ASSERT_TRUE(result_first.has_value());
  EXPECT_EQ(result_first->first, "/lib/lib0.so");
  
  uintptr_t last_addr = 0x7f0000000000ULL + (999 * 0x100000);
  auto result_last = map.resolve(last_addr);
  ASSERT_TRUE(result_last.has_value());
  EXPECT_EQ(result_last->first, "/lib/lib999.so");
  
  cleanupTestFile(filename);
}

TEST_F(LibraryMapTest, LibraryInfoConstructor) {
  LibraryMap::LibraryInfo info1;
  EXPECT_TRUE(info1.name.empty());
  EXPECT_EQ(info1.base, 0u);
  EXPECT_EQ(info1.end, 0u);
  EXPECT_FALSE(info1.is_executable);
  
  LibraryMap::LibraryInfo info2("/lib/test.so", 0x1000, 0x2000, true);
  EXPECT_EQ(info2.name, "/lib/test.so");
  EXPECT_EQ(info2.base, 0x1000u);
  EXPECT_EQ(info2.end, 0x2000u);
  EXPECT_TRUE(info2.is_executable);
}
