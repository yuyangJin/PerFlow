// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <cstdio>

#include "sampling/data_export.h"
#include "sampling/library_map.h"

using namespace perflow::sampling;

class LibraryMapExportTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
  
  void CleanupTestFile(const char* filepath) {
    std::remove(filepath);
  }
};

TEST_F(LibraryMapExportTest, ExportAndImport) {
  LibraryMap original_map;
  
  // Add some test libraries
  LibraryMap::LibraryInfo lib1("/test/lib1.so", 0x1000, 0x2000, true);
  LibraryMap::LibraryInfo lib2("/test/lib2.so", 0x3000, 0x4000, true);
  LibraryMap::LibraryInfo lib3("/usr/bin/test", 0x400000, 0x500000, true);
  
  original_map.add_library(lib1);
  original_map.add_library(lib2);
  original_map.add_library(lib3);
  
  // Export the map
  const char* test_dir = "/tmp";
  const char* test_name = "test_libmap";
  LibraryMapExporter exporter(test_dir, test_name);
  
  DataResult export_result = exporter.exportMap(original_map, 42);
  ASSERT_EQ(export_result, DataResult::kSuccess);
  
  // Import the map
  LibraryMapImporter importer(exporter.filepath());
  LibraryMap imported_map;
  uint32_t process_id = 0;
  
  DataResult import_result = importer.importMap(imported_map, &process_id);
  ASSERT_EQ(import_result, DataResult::kSuccess);
  
  // Verify process ID
  EXPECT_EQ(process_id, 42u);
  
  // Verify the imported map has the same number of libraries
  EXPECT_EQ(imported_map.size(), original_map.size());
  
  // Verify resolution works the same
  auto result1 = imported_map.resolve(0x1500);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(result1->first, "/test/lib1.so");
  EXPECT_EQ(result1->second, 0x500u);
  
  auto result2 = imported_map.resolve(0x3500);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2->first, "/test/lib2.so");
  EXPECT_EQ(result2->second, 0x500u);
  
  auto result3 = imported_map.resolve(0x450000);
  ASSERT_TRUE(result3.has_value());
  EXPECT_EQ(result3->first, "/usr/bin/test");
  EXPECT_EQ(result3->second, 0x50000u);
  
  // Clean up
  CleanupTestFile(exporter.filepath());
}

TEST_F(LibraryMapExportTest, ExportEmptyMap) {
  LibraryMap empty_map;
  
  const char* test_dir = "/tmp";
  const char* test_name = "test_empty_libmap";
  LibraryMapExporter exporter(test_dir, test_name);
  
  DataResult result = exporter.exportMap(empty_map, 0);
  EXPECT_EQ(result, DataResult::kSuccess);
  
  // Import it back
  LibraryMapImporter importer(exporter.filepath());
  LibraryMap imported_map;
  
  DataResult import_result = importer.importMap(imported_map, nullptr);
  EXPECT_EQ(import_result, DataResult::kSuccess);
  EXPECT_TRUE(imported_map.empty());
  
  // Clean up
  CleanupTestFile(exporter.filepath());
}

TEST_F(LibraryMapExportTest, ImportNonexistentFile) {
  LibraryMapImporter importer("/tmp/nonexistent_libmap_file.libmap");
  LibraryMap map;
  
  DataResult result = importer.importMap(map, nullptr);
  EXPECT_EQ(result, DataResult::kErrorFileOpen);
}

TEST_F(LibraryMapExportTest, ExportWithLongLibraryName) {
  LibraryMap map;
  
  // Create a library with a very long name
  std::string long_name = "/very/long/path/to/library/";
  for (int i = 0; i < 50; ++i) {
    long_name += "subdir/";
  }
  long_name += "libtest.so";
  
  LibraryMap::LibraryInfo lib(long_name, 0x1000, 0x2000, true);
  map.add_library(lib);
  
  // Export
  const char* test_dir = "/tmp";
  const char* test_name = "test_long_name_libmap";
  LibraryMapExporter exporter(test_dir, test_name);
  
  DataResult export_result = exporter.exportMap(map, 0);
  ASSERT_EQ(export_result, DataResult::kSuccess);
  
  // Import and verify
  LibraryMapImporter importer(exporter.filepath());
  LibraryMap imported_map;
  
  DataResult import_result = importer.importMap(imported_map, nullptr);
  ASSERT_EQ(import_result, DataResult::kSuccess);
  
  EXPECT_EQ(imported_map.size(), 1u);
  
  auto result = imported_map.resolve(0x1500);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, long_name);
  
  // Clean up
  CleanupTestFile(exporter.filepath());
}

TEST_F(LibraryMapExportTest, FilepathConstruction) {
  LibraryMapExporter exporter("/tmp", "testfile");
  
  // Check that filepath was constructed correctly
  std::string filepath = exporter.filepath();
  EXPECT_EQ(filepath, "/tmp/testfile.libmap");
}
