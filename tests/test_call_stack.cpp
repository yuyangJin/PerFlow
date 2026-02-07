// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "sampling/call_stack.h"

using namespace perflow::sampling;

class CallStackTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(CallStackTest, DefaultConstruction) {
  CallStack<> stack;
  EXPECT_EQ(stack.depth(), 0u);
  EXPECT_TRUE(stack.empty());
  EXPECT_FALSE(stack.full());
  EXPECT_EQ(stack.max_depth(), kDefaultMaxStackDepth);
}

TEST_F(CallStackTest, ConstructionFromArray) {
  uintptr_t addresses[] = {0x1000, 0x2000, 0x3000};
  CallStack<> stack(addresses, 3);

  EXPECT_EQ(stack.depth(), 3u);
  EXPECT_FALSE(stack.empty());
  EXPECT_EQ(stack.frame(0), 0x1000u);
  EXPECT_EQ(stack.frame(1), 0x2000u);
  EXPECT_EQ(stack.frame(2), 0x3000u);
}

TEST_F(CallStackTest, PushAndPop) {
  CallStack<4> stack;

  EXPECT_TRUE(stack.push(0x1000));
  EXPECT_TRUE(stack.push(0x2000));
  EXPECT_TRUE(stack.push(0x3000));
  EXPECT_TRUE(stack.push(0x4000));
  EXPECT_FALSE(stack.push(0x5000));  // Should fail - stack is full

  EXPECT_TRUE(stack.full());
  EXPECT_EQ(stack.depth(), 4u);

  EXPECT_EQ(stack.pop(), 0x4000u);
  EXPECT_EQ(stack.pop(), 0x3000u);
  EXPECT_EQ(stack.depth(), 2u);

  stack.clear();
  EXPECT_TRUE(stack.empty());
  EXPECT_EQ(stack.pop(), 0u);  // Pop from empty returns 0
}

TEST_F(CallStackTest, HashConsistency) {
  uintptr_t addresses[] = {0x1000, 0x2000, 0x3000};
  CallStack<> stack1(addresses, 3);
  CallStack<> stack2(addresses, 3);

  // Same content should produce same hash
  EXPECT_EQ(stack1.hash(), stack2.hash());

  // Different content should (likely) produce different hash
  stack2.push(0x4000);
  EXPECT_NE(stack1.hash(), stack2.hash());
}

TEST_F(CallStackTest, Equality) {
  uintptr_t addresses1[] = {0x1000, 0x2000, 0x3000};
  uintptr_t addresses2[] = {0x1000, 0x2000, 0x3000};
  uintptr_t addresses3[] = {0x1000, 0x2000, 0x4000};

  CallStack<> stack1(addresses1, 3);
  CallStack<> stack2(addresses2, 3);
  CallStack<> stack3(addresses3, 3);

  EXPECT_TRUE(stack1 == stack2);
  EXPECT_FALSE(stack1 != stack2);
  EXPECT_FALSE(stack1 == stack3);
  EXPECT_TRUE(stack1 != stack3);
}

TEST_F(CallStackTest, CopyConstruction) {
  uintptr_t addresses[] = {0x1000, 0x2000, 0x3000};
  CallStack<> stack1(addresses, 3);
  CallStack<> stack2(stack1);

  EXPECT_EQ(stack1, stack2);
  EXPECT_EQ(stack1.depth(), stack2.depth());
  EXPECT_EQ(stack1.hash(), stack2.hash());
}

TEST_F(CallStackTest, CopyAssignment) {
  uintptr_t addresses1[] = {0x1000, 0x2000, 0x3000};
  uintptr_t addresses2[] = {0x4000, 0x5000};

  CallStack<> stack1(addresses1, 3);
  CallStack<> stack2(addresses2, 2);

  stack2 = stack1;

  EXPECT_EQ(stack1, stack2);
  EXPECT_EQ(stack2.depth(), 3u);
}

TEST_F(CallStackTest, SetMethod) {
  CallStack<> stack;
  uintptr_t addresses[] = {0x1000, 0x2000};

  stack.set(addresses, 2);
  EXPECT_EQ(stack.depth(), 2u);
  EXPECT_EQ(stack.frame(0), 0x1000u);
  EXPECT_EQ(stack.frame(1), 0x2000u);

  // Set with larger array - should truncate
  uintptr_t large_addresses[200];
  for (size_t i = 0; i < 200; ++i) {
    large_addresses[i] = i * 0x1000;
  }
  stack.set(large_addresses, 200);
  EXPECT_EQ(stack.depth(), kDefaultMaxStackDepth);
}

TEST_F(CallStackTest, FrameAccessOutOfBounds) {
  uintptr_t addresses[] = {0x1000, 0x2000};
  CallStack<> stack(addresses, 2);

  EXPECT_EQ(stack.frame(0), 0x1000u);
  EXPECT_EQ(stack.frame(1), 0x2000u);
  EXPECT_EQ(stack.frame(2), 0u);  // Out of bounds
  EXPECT_EQ(stack.frame(100), 0u);  // Way out of bounds
}

TEST_F(CallStackTest, HashCaching) {
  uintptr_t addresses[] = {0x1000, 0x2000, 0x3000};
  CallStack<> stack(addresses, 3);

  // First hash computation
  size_t hash1 = stack.hash();
  // Second call should use cached value
  size_t hash2 = stack.hash();

  EXPECT_EQ(hash1, hash2);

  // After modification, hash should be different
  stack.push(0x4000);
  size_t hash3 = stack.hash();
  EXPECT_NE(hash1, hash3);
}

TEST_F(CallStackTest, EmptyStackHash) {
  CallStack<> stack1;
  CallStack<> stack2;

  // Two empty stacks should have the same hash
  EXPECT_EQ(stack1.hash(), stack2.hash());
}

TEST_F(CallStackTest, CustomMaxDepth) {
  CallStack<8> small_stack;
  EXPECT_EQ(small_stack.max_depth(), 8u);

  CallStack<256> large_stack;
  EXPECT_EQ(large_stack.max_depth(), 256u);
}
