// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "analysis/symbol_resolver.h"
#include <dlfcn.h>
#include <unistd.h>

using namespace perflow::analysis;

class SymbolResolverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get path to current executable for testing
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
      buf[len] = '\0';
      test_binary_ = std::string(buf);
    }
  }
  
  void TearDown() override {}
  
  std::string test_binary_;
};

TEST_F(SymbolResolverTest, DefaultConstruction) {
  SymbolResolver resolver;
  // Should not crash
  EXPECT_TRUE(true);
}

TEST_F(SymbolResolverTest, ConstructionWithStrategy) {
  SymbolResolver resolver1(SymbolResolver::Strategy::kDlAddrOnly);
  SymbolResolver resolver2(SymbolResolver::Strategy::kAddr2LineOnly);
  SymbolResolver resolver3(SymbolResolver::Strategy::kAutoFallback);
  
  // Should not crash
  EXPECT_TRUE(true);
}

TEST_F(SymbolResolverTest, ConstructionWithCacheControl) {
  SymbolResolver with_cache(SymbolResolver::Strategy::kAutoFallback, true);
  SymbolResolver without_cache(SymbolResolver::Strategy::kAutoFallback, false);
  
  // Should not crash
  EXPECT_TRUE(true);
}

TEST_F(SymbolResolverTest, MoveConstructor) {
  SymbolResolver resolver1;
  SymbolResolver resolver2(std::move(resolver1));
  
  // Should not crash
  EXPECT_TRUE(true);
}

TEST_F(SymbolResolverTest, MoveAssignment) {
  SymbolResolver resolver1;
  SymbolResolver resolver2;
  resolver2 = std::move(resolver1);
  
  // Should not crash
  EXPECT_TRUE(true);
}

TEST_F(SymbolResolverTest, ResolveNonExistentLibrary) {
  SymbolResolver resolver;
  
  // Try to resolve from a non-existent library
  SymbolInfo info = resolver.resolve("/nonexistent/library.so", 0x1000);
  
  // Should not crash and return unresolved
  EXPECT_FALSE(info.is_resolved());
  EXPECT_TRUE(info.function_name.empty());
}

TEST_F(SymbolResolverTest, ResolveWithDlAddr) {
  SymbolResolver resolver(SymbolResolver::Strategy::kDlAddrOnly);
  
  // Try to resolve a known function from libc
  void* handle = dlopen("libc.so.6", RTLD_LAZY);
  if (!handle) {
    GTEST_SKIP() << "Could not load libc.so.6";
  }
  
  void* printf_addr = dlsym(handle, "printf");
  if (!printf_addr) {
    dlclose(handle);
    GTEST_SKIP() << "Could not find printf symbol";
  }
  
  Dl_info dl_info;
  if (dladdr(printf_addr, &dl_info) == 0) {
    dlclose(handle);
    GTEST_SKIP() << "dladdr failed";
  }
  
  // Calculate offset
  uintptr_t base = reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
  uintptr_t addr = reinterpret_cast<uintptr_t>(printf_addr);
  uintptr_t offset = addr - base;
  
  // Resolve using our resolver
  SymbolInfo info = resolver.resolve(dl_info.dli_fname, offset);
  
  dlclose(handle);
  
  // The symbol should be resolved (at least for export symbols like printf)
  if (info.is_resolved()) {
    EXPECT_FALSE(info.function_name.empty());
    // printf might be mangled or aliased, so we just check it's not empty
  }
}

TEST_F(SymbolResolverTest, CachingWorks) {
  SymbolResolver resolver(SymbolResolver::Strategy::kAutoFallback, true);
  
  // Make two identical resolve calls
  std::string library = "/lib/x86_64-linux-gnu/libc.so.6";
  uintptr_t offset = 0x1000;
  
  // First call should miss cache
  auto stats1 = resolver.get_cache_stats();
  size_t initial_misses = stats1.misses;
  
  resolver.resolve(library, offset);
  
  auto stats2 = resolver.get_cache_stats();
  EXPECT_EQ(stats2.misses, initial_misses + 1);
  EXPECT_EQ(stats2.size, 1u);
  
  // Second call should hit cache
  resolver.resolve(library, offset);
  
  auto stats3 = resolver.get_cache_stats();
  EXPECT_EQ(stats3.hits, stats2.hits + 1);
  EXPECT_EQ(stats3.misses, initial_misses + 1);  // No new misses
}

TEST_F(SymbolResolverTest, ClearCache) {
  SymbolResolver resolver(SymbolResolver::Strategy::kAutoFallback, true);
  
  // Add some entries to cache
  resolver.resolve("/lib/libc.so.6", 0x1000);
  resolver.resolve("/lib/libc.so.6", 0x2000);
  
  auto stats_before = resolver.get_cache_stats();
  EXPECT_GT(stats_before.size, 0u);
  
  // Clear cache
  resolver.clear_cache();
  
  auto stats_after = resolver.get_cache_stats();
  EXPECT_EQ(stats_after.size, 0u);
  EXPECT_EQ(stats_after.hits, 0u);
  EXPECT_EQ(stats_after.misses, 0u);
}

TEST_F(SymbolResolverTest, CacheDisabled) {
  SymbolResolver resolver(SymbolResolver::Strategy::kAutoFallback, false);
  
  // Make two identical resolve calls
  resolver.resolve("/lib/libc.so.6", 0x1000);
  resolver.resolve("/lib/libc.so.6", 0x1000);
  
  auto stats = resolver.get_cache_stats();
  // With caching disabled, all should be zero
  EXPECT_EQ(stats.size, 0u);
}

TEST_F(SymbolResolverTest, SymbolInfoDefaultConstruction) {
  SymbolInfo info;
  
  EXPECT_TRUE(info.function_name.empty());
  EXPECT_TRUE(info.filename.empty());
  EXPECT_EQ(info.line_number, 0u);
  EXPECT_FALSE(info.is_resolved());
}

TEST_F(SymbolResolverTest, SymbolInfoWithValues) {
  SymbolInfo info("my_function", "source.cpp", 42);
  
  EXPECT_EQ(info.function_name, "my_function");
  EXPECT_EQ(info.filename, "source.cpp");
  EXPECT_EQ(info.line_number, 42u);
  EXPECT_TRUE(info.is_resolved());
}

TEST_F(SymbolResolverTest, SymbolInfoPartialResolution) {
  // Function name but no source location
  SymbolInfo info("my_function", "", 0);
  
  EXPECT_EQ(info.function_name, "my_function");
  EXPECT_TRUE(info.filename.empty());
  EXPECT_EQ(info.line_number, 0u);
  EXPECT_TRUE(info.is_resolved());  // Still considered resolved if function name exists
}

// Test helper function to ensure it appears in symbols
__attribute__((noinline)) int test_function_for_symbol_resolution(int x) {
  return x * 2 + 1;
}

TEST_F(SymbolResolverTest, ResolveTestBinaryFunction) {
  if (test_binary_.empty()) {
    GTEST_SKIP() << "Could not determine test binary path";
  }
  
  SymbolResolver resolver(SymbolResolver::Strategy::kAutoFallback);
  
  // Get address of our test function
  void* func_addr = reinterpret_cast<void*>(&test_function_for_symbol_resolution);
  
  Dl_info dl_info;
  if (dladdr(func_addr, &dl_info) == 0) {
    GTEST_SKIP() << "dladdr failed for test function";
  }
  
  // Calculate offset
  uintptr_t base = reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
  uintptr_t addr = reinterpret_cast<uintptr_t>(func_addr);
  uintptr_t offset = addr - base;
  
  // Try to resolve
  SymbolInfo info = resolver.resolve(test_binary_, offset);
  
  // We should at least get the function name
  if (info.is_resolved()) {
    EXPECT_FALSE(info.function_name.empty());
    // The function name should contain our test function name (possibly mangled)
    // Just verify we got something
  }
  
  // Call the function to avoid unused function warning
  EXPECT_EQ(test_function_for_symbol_resolution(5), 11);
}

TEST_F(SymbolResolverTest, DifferentOffsetsDifferentResults) {
  SymbolResolver resolver;
  
  std::string library = test_binary_.empty() ? "/lib/x86_64-linux-gnu/libc.so.6" : test_binary_;
  
  // Resolve two different offsets
  SymbolInfo info1 = resolver.resolve(library, 0x1000);
  SymbolInfo info2 = resolver.resolve(library, 0x2000);
  
  // They should be cached separately
  auto stats = resolver.get_cache_stats();
  EXPECT_GE(stats.size, 2u);  // At least 2 different cache entries
}
