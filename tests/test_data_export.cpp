// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

#include "sampling/data_export.h"

using namespace perflow::sampling;

class DataExportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test directory
    std::system("mkdir -p /tmp/perflow_test");
  }

  void TearDown() override {
    // Cleanup test files
    std::system("rm -f /tmp/perflow_test/*.pflw");
    std::system("rm -f /tmp/perflow_test/*.pflw.gz");
  }
};

TEST_F(DataExportTest, HeaderSize) {
  // Verify header is exactly 64 bytes
  EXPECT_EQ(sizeof(DataFileHeader), 64u);
  EXPECT_EQ(sizeof(DataEntryHeader), 16u);
}

TEST_F(DataExportTest, HeaderDefaults) {
  DataFileHeader header;

  EXPECT_EQ(header.magic, kPerFlowMagic);
  EXPECT_EQ(header.version, kDataFormatVersion);
  EXPECT_EQ(header.compression,
            static_cast<uint8_t>(CompressionType::kNone));
  EXPECT_EQ(header.entry_count, 0u);
  EXPECT_EQ(header.max_stack_depth, kDefaultMaxStackDepth);
}

TEST_F(DataExportTest, ExportEmptyMap) {
  StaticHashMap<CallStack<>, uint64_t, 64> data;

  DataExporter exporter("/tmp/perflow_test", "test_empty", false);
  DataResult result = exporter.exportData(data);

  EXPECT_EQ(result, DataResult::kSuccess);

  // Verify file exists
  FILE* f = std::fopen("/tmp/perflow_test/test_empty.pflw", "rb");
  ASSERT_NE(f, nullptr);

  // Read and verify header
  DataFileHeader header;
  EXPECT_EQ(std::fread(&header, sizeof(header), 1, f), 1u);
  EXPECT_EQ(header.magic, kPerFlowMagic);
  EXPECT_EQ(header.entry_count, 0u);

  std::fclose(f);
}

TEST_F(DataExportTest, ExportAndImport) {
  // Create test data
  StaticHashMap<CallStack<32>, uint64_t, 64> export_data;

  uintptr_t stack1_addrs[] = {0x1000, 0x2000, 0x3000};
  uintptr_t stack2_addrs[] = {0x4000, 0x5000};
  uintptr_t stack3_addrs[] = {0x6000};

  CallStack<32> stack1(stack1_addrs, 3);
  CallStack<32> stack2(stack2_addrs, 2);
  CallStack<32> stack3(stack3_addrs, 1);

  export_data.insert(stack1, 100);
  export_data.insert(stack2, 200);
  export_data.insert(stack3, 50);

  // Export
  DataExporter exporter("/tmp/perflow_test", "test_export", false);
  DataResult export_result = exporter.exportData(export_data);
  EXPECT_EQ(export_result, DataResult::kSuccess);

  // Import
  StaticHashMap<CallStack<32>, uint64_t, 64> import_data;
  DataImporter importer("/tmp/perflow_test/test_export.pflw");
  DataResult import_result = importer.importData(import_data);
  EXPECT_EQ(import_result, DataResult::kSuccess);

  // Verify data
  EXPECT_EQ(import_data.size(), 3u);

  uint64_t* val1 = import_data.find(stack1);
  uint64_t* val2 = import_data.find(stack2);
  uint64_t* val3 = import_data.find(stack3);

  ASSERT_NE(val1, nullptr);
  ASSERT_NE(val2, nullptr);
  ASSERT_NE(val3, nullptr);

  EXPECT_EQ(*val1, 100u);
  EXPECT_EQ(*val2, 200u);
  EXPECT_EQ(*val3, 50u);
}

TEST_F(DataExportTest, ReadHeader) {
  // Create a test file
  StaticHashMap<CallStack<>, uint64_t, 64> data;
  uintptr_t addrs[] = {0x1000, 0x2000};
  CallStack<> stack(addrs, 2);
  data.insert(stack, 42);

  DataExporter exporter("/tmp/perflow_test", "test_header", false);
  EXPECT_EQ(exporter.exportData(data), DataResult::kSuccess);

  // Read just the header
  DataImporter importer("/tmp/perflow_test/test_header.pflw");
  DataFileHeader header;
  DataResult result = importer.readHeader(header);

  EXPECT_EQ(result, DataResult::kSuccess);
  EXPECT_EQ(header.magic, kPerFlowMagic);
  EXPECT_EQ(header.entry_count, 1u);
  EXPECT_EQ(header.max_stack_depth, kDefaultMaxStackDepth);
}

TEST_F(DataExportTest, ImportNonExistentFile) {
  StaticHashMap<CallStack<>, uint64_t, 64> data;
  DataImporter importer("/tmp/perflow_test/nonexistent.pflw");
  DataResult result = importer.importData(data);

  EXPECT_EQ(result, DataResult::kErrorFileOpen);
}

TEST_F(DataExportTest, ImportInvalidFile) {
  // Create an invalid file with 64 bytes but invalid magic
  FILE* f = std::fopen("/tmp/perflow_test/invalid.pflw", "wb");
  ASSERT_NE(f, nullptr);
  char garbage[64];
  std::memset(garbage, 'X', sizeof(garbage));  // Fill with 'X' - invalid magic
  std::fwrite(garbage, sizeof(garbage), 1, f);
  std::fclose(f);

  StaticHashMap<CallStack<>, uint64_t, 64> data;
  DataImporter importer("/tmp/perflow_test/invalid.pflw");
  DataResult result = importer.importData(data);

  EXPECT_EQ(result, DataResult::kErrorInvalidFormat);
}

TEST_F(DataExportTest, FilepathGeneration) {
  DataExporter exporter1("/tmp/perflow_test", "mydata", false);
  EXPECT_STREQ(exporter1.filepath(), "/tmp/perflow_test/mydata.pflw");

  DataExporter exporter2("/tmp/perflow_test/", "mydata", false);
  EXPECT_STREQ(exporter2.filepath(), "/tmp/perflow_test/mydata.pflw");

  DataExporter exporter3("/tmp/perflow_test", "mydata", true);
  EXPECT_STREQ(exporter3.filepath(), "/tmp/perflow_test/mydata.pflw.gz");
}

TEST_F(DataExportTest, LargeDataset) {
  StaticHashMap<CallStack<16>, uint64_t, 1024> data;

  // Create many unique call stacks
  for (size_t i = 0; i < 500; ++i) {
    uintptr_t addrs[4];
    addrs[0] = 0x1000 + i;
    addrs[1] = 0x2000 + i;
    addrs[2] = 0x3000 + i;
    addrs[3] = 0x4000 + i;

    CallStack<16> stack(addrs, 4);
    data.insert(stack, i * 10);
  }

  EXPECT_EQ(data.size(), 500u);

  // Export
  DataExporter exporter("/tmp/perflow_test", "test_large", false);
  EXPECT_EQ(exporter.exportData(data), DataResult::kSuccess);

  // Import
  StaticHashMap<CallStack<16>, uint64_t, 1024> imported;
  DataImporter importer("/tmp/perflow_test/test_large.pflw");
  EXPECT_EQ(importer.importData(imported), DataResult::kSuccess);

  EXPECT_EQ(imported.size(), 500u);
}
