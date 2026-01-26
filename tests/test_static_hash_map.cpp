// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "sampling/static_hash_map.h"

using namespace perflow::sampling;

// Simple key type for testing
struct SimpleKey {
  int value;

  SimpleKey() noexcept : value(0) {}
  explicit SimpleKey(int v) noexcept : value(v) {}

  bool operator==(const SimpleKey& other) const noexcept {
    return value == other.value;
  }

  size_t hash() const noexcept {
    return static_cast<size_t>(value) * 2654435761u;
  }
};

class StaticHashMapTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(StaticHashMapTest, DefaultConstruction) {
  StaticHashMap<SimpleKey, int, 16> map;
  EXPECT_EQ(map.size(), 0u);
  EXPECT_TRUE(map.empty());
  EXPECT_FALSE(map.full());
  EXPECT_EQ(map.capacity(), 16u);
}

TEST_F(StaticHashMapTest, InsertAndFind) {
  StaticHashMap<SimpleKey, int, 32> map;

  SimpleKey key1(1);
  SimpleKey key2(2);
  SimpleKey key3(3);

  EXPECT_TRUE(map.insert(key1, 100));
  EXPECT_TRUE(map.insert(key2, 200));
  EXPECT_TRUE(map.insert(key3, 300));

  EXPECT_EQ(map.size(), 3u);

  int* val1 = map.find(key1);
  int* val2 = map.find(key2);
  int* val3 = map.find(key3);

  ASSERT_NE(val1, nullptr);
  ASSERT_NE(val2, nullptr);
  ASSERT_NE(val3, nullptr);

  EXPECT_EQ(*val1, 100);
  EXPECT_EQ(*val2, 200);
  EXPECT_EQ(*val3, 300);
}

TEST_F(StaticHashMapTest, FindNonExistent) {
  StaticHashMap<SimpleKey, int, 16> map;

  SimpleKey key(42);
  EXPECT_EQ(map.find(key), nullptr);

  map.insert(key, 100);
  SimpleKey other(99);
  EXPECT_EQ(map.find(other), nullptr);
}

TEST_F(StaticHashMapTest, UpdateExisting) {
  StaticHashMap<SimpleKey, int, 16> map;

  SimpleKey key(1);
  EXPECT_TRUE(map.insert(key, 100));
  EXPECT_EQ(map.size(), 1u);

  EXPECT_TRUE(map.insert(key, 200));  // Update
  EXPECT_EQ(map.size(), 1u);  // Size should not change

  int* val = map.find(key);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 200);
}

TEST_F(StaticHashMapTest, InsertOrGet) {
  StaticHashMap<SimpleKey, int, 16> map;

  SimpleKey key(1);

  // First call creates new entry with default value
  int* val1 = map.insert_or_get(key);
  ASSERT_NE(val1, nullptr);
  EXPECT_EQ(*val1, 0);  // Default initialized

  *val1 = 100;

  // Second call returns existing entry
  int* val2 = map.insert_or_get(key);
  ASSERT_NE(val2, nullptr);
  EXPECT_EQ(*val2, 100);
  EXPECT_EQ(val1, val2);  // Same pointer
}

TEST_F(StaticHashMapTest, Increment) {
  StaticHashMap<SimpleKey, int, 16> map;

  SimpleKey key(1);

  EXPECT_TRUE(map.increment(key));
  EXPECT_EQ(map.size(), 1u);

  int* val = map.find(key);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 1);

  EXPECT_TRUE(map.increment(key));
  EXPECT_TRUE(map.increment(key));
  EXPECT_EQ(*val, 3);
}

TEST_F(StaticHashMapTest, Add) {
  StaticHashMap<SimpleKey, int, 16> map;

  SimpleKey key(1);

  EXPECT_TRUE(map.add(key, 10));
  int* val = map.find(key);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 10);

  EXPECT_TRUE(map.add(key, 5));
  EXPECT_EQ(*val, 15);
}

TEST_F(StaticHashMapTest, Erase) {
  StaticHashMap<SimpleKey, int, 16> map;

  SimpleKey key1(1);
  SimpleKey key2(2);

  map.insert(key1, 100);
  map.insert(key2, 200);
  EXPECT_EQ(map.size(), 2u);

  EXPECT_TRUE(map.erase(key1));
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map.find(key1), nullptr);
  EXPECT_NE(map.find(key2), nullptr);

  EXPECT_FALSE(map.erase(key1));  // Already erased
}

TEST_F(StaticHashMapTest, Clear) {
  StaticHashMap<SimpleKey, int, 16> map;

  for (int i = 0; i < 10; ++i) {
    map.insert(SimpleKey(i), i * 100);
  }
  EXPECT_EQ(map.size(), 10u);

  map.clear();
  EXPECT_EQ(map.size(), 0u);
  EXPECT_TRUE(map.empty());

  // Should be able to insert after clear
  EXPECT_TRUE(map.insert(SimpleKey(1), 100));
  EXPECT_EQ(map.size(), 1u);
}

TEST_F(StaticHashMapTest, ForEach) {
  StaticHashMap<SimpleKey, int, 16> map;

  map.insert(SimpleKey(1), 100);
  map.insert(SimpleKey(2), 200);
  map.insert(SimpleKey(3), 300);

  int sum = 0;
  int count = 0;
  map.for_each([&sum, &count](const SimpleKey& key, int& value) {
    (void)key;
    sum += value;
    ++count;
  });

  EXPECT_EQ(count, 3);
  EXPECT_EQ(sum, 600);
}

TEST_F(StaticHashMapTest, CollisionHandling) {
  // Use a small map to force collisions
  StaticHashMap<SimpleKey, int, 8> map;

  // Insert more items than would fit without collision handling
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(map.insert(SimpleKey(i), i * 100));
  }

  // Verify all items are findable
  for (int i = 0; i < 6; ++i) {
    int* val = map.find(SimpleKey(i));
    ASSERT_NE(val, nullptr) << "Key " << i << " not found";
    EXPECT_EQ(*val, i * 100);
  }
}

TEST_F(StaticHashMapTest, FullMap) {
  StaticHashMap<SimpleKey, int, 4> map;

  EXPECT_TRUE(map.insert(SimpleKey(1), 100));
  EXPECT_TRUE(map.insert(SimpleKey(2), 200));
  EXPECT_TRUE(map.insert(SimpleKey(3), 300));
  EXPECT_TRUE(map.insert(SimpleKey(4), 400));

  EXPECT_TRUE(map.full());

  // Should fail to insert new key
  EXPECT_FALSE(map.insert(SimpleKey(5), 500));

  // But should be able to update existing key
  EXPECT_TRUE(map.insert(SimpleKey(1), 999));
  int* val = map.find(SimpleKey(1));
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 999);
}

TEST_F(StaticHashMapTest, InsertAfterErase) {
  StaticHashMap<SimpleKey, int, 4> map;

  map.insert(SimpleKey(1), 100);
  map.insert(SimpleKey(2), 200);
  map.insert(SimpleKey(3), 300);

  map.erase(SimpleKey(2));

  // Should be able to insert in the deleted slot
  EXPECT_TRUE(map.insert(SimpleKey(4), 400));
  EXPECT_NE(map.find(SimpleKey(4)), nullptr);
}

TEST_F(StaticHashMapTest, ConstFind) {
  StaticHashMap<SimpleKey, int, 16> map;
  map.insert(SimpleKey(1), 100);

  const auto& const_map = map;
  const int* val = const_map.find(SimpleKey(1));
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 100);
}
