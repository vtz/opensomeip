/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <gtest/gtest.h>

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/memory.h"
#include "static_config.h"

#include <cstdint>
#include <vector>

namespace someip::platform {
namespace {

class StaticAllocEnv : public ::testing::Environment {
public:
    void SetUp() override { init_static_allocator(); }
};
[[maybe_unused]] auto* const g_env =
    ::testing::AddGlobalTestEnvironment(new StaticAllocEnv());

// --- ByteBuffer tests ---

class ByteBufferTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

/**
 * @test_case TC_BYTEBUF_DEFAULT
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, DefaultConstructEmpty) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_EQ(buf.data(), nullptr);
}

/**
 * @test_case TC_BYTEBUF_INIT_LIST
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, InitializerListConstruct) {
    ByteBuffer buf{0x01, 0x02, 0x03};
    ASSERT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(buf[2], 0x03);
}

/**
 * @test_case TC_BYTEBUF_SIZE_VAL
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, SizeValueConstruct) {
    ByteBuffer buf(10, 0xAA);
    ASSERT_EQ(buf.size(), 10u);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(buf[i], 0xAA);
    }
}

/**
 * @test_case TC_BYTEBUF_PUSH_BACK
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, PushBack) {
    ByteBuffer buf;
    buf.push_back(0x42);
    buf.push_back(0x43);
    ASSERT_EQ(buf.size(), 2u);
    EXPECT_EQ(buf[0], 0x42);
    EXPECT_EQ(buf[1], 0x43);
}

/**
 * @test_case TC_BYTEBUF_RESIZE
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, Resize) {
    ByteBuffer buf{0x01, 0x02};
    buf.resize(5);
    ASSERT_EQ(buf.size(), 5u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(buf[2], 0x00);

    buf.resize(1);
    ASSERT_EQ(buf.size(), 1u);
    EXPECT_EQ(buf[0], 0x01);
}

/**
 * @test_case TC_BYTEBUF_RESIZE_VAL
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, ResizeWithValue) {
    ByteBuffer buf;
    buf.resize(4, 0xFF);
    ASSERT_EQ(buf.size(), 4u);
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(buf[i], 0xFF);
    }
}

/**
 * @test_case TC_BYTEBUF_CLEAR
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, Clear) {
    ByteBuffer buf{0x01, 0x02, 0x03};
    buf.clear();
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_TRUE(buf.empty());
    EXPECT_GE(buf.capacity(), 3u);
}

/**
 * @test_case TC_BYTEBUF_RESERVE
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, Reserve) {
    ByteBuffer buf;
    buf.reserve(100);
    EXPECT_GE(buf.capacity(), 100u);
    EXPECT_EQ(buf.size(), 0u);
}

/**
 * @test_case TC_BYTEBUF_MOVE_CONSTRUCT
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, MoveConstruct) {
    ByteBuffer a{0x01, 0x02, 0x03};
    ByteBuffer b(std::move(a));
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_TRUE(a.empty()); // NOLINT(bugprone-use-after-move)
}

/**
 * @test_case TC_BYTEBUF_MOVE_ASSIGN
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, MoveAssign) {
    ByteBuffer a{0x01, 0x02};
    ByteBuffer b{0x03, 0x04, 0x05};
    b = std::move(a);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x01);
}

/**
 * @test_case TC_BYTEBUF_COPY_CONSTRUCT
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, CopyConstruct) {
    ByteBuffer a{0x01, 0x02, 0x03};
    ByteBuffer b(a);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_EQ(b[2], 0x03);

    b[0] = 0xFF;
    EXPECT_EQ(a[0], 0x01);
}

/**
 * @test_case TC_BYTEBUF_COPY_ASSIGN
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, CopyAssign) {
    ByteBuffer a{0x01, 0x02};
    ByteBuffer b;
    b = a;
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[1], 0x02);
}

/**
 * @test_case TC_BYTEBUF_ITERATOR
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, IteratorRange) {
    ByteBuffer buf{0x10, 0x20, 0x30};
    std::vector<uint8_t> vec(buf.begin(), buf.end());
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 0x10);
    EXPECT_EQ(vec[2], 0x30);
}

/**
 * @test_case TC_BYTEBUF_EQUALITY
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, Equality) {
    ByteBuffer a{0x01, 0x02};
    ByteBuffer b{0x01, 0x02};
    ByteBuffer c{0x01, 0x03};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

/**
 * @test_case TC_BYTEBUF_INSERT
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(ByteBufferTest, Insert) {
    ByteBuffer buf{0x01, 0x04};
    uint8_t mid[] = {0x02, 0x03};
    buf.insert(buf.begin() + 1, mid, mid + 2);
    ASSERT_EQ(buf.size(), 4u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(buf[2], 0x03);
    EXPECT_EQ(buf[3], 0x04);
}

// --- Container conformance tests ---

/**
 * @test_case TC_CONTAINER_VECTOR_PUSH_BACK
 * @tests REQ_PAL_CONTAINER_VECTOR
 */
TEST(ContainerTest, VectorPushBack) {
    Vector<int, 8> v;
    for (int i = 0; i < 8; ++i) {
        v.push_back(i);
    }
    EXPECT_EQ(v.size(), 8U);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(v[i], i);
    }
    int sum = 0;
    for (auto val : v) {
        sum += val;
    }
    EXPECT_EQ(sum, 28);
}

/**
 * @test_case TC_CONTAINER_STRING_APPEND
 * @tests REQ_PAL_CONTAINER_STRING
 */
TEST(ContainerTest, StringAppend) {
    String<32> s;
    s.append("hello");
    s.append(" world");
    EXPECT_EQ(s.size(), 11U);
    EXPECT_STREQ(s.c_str(), "hello world");
    EXPECT_TRUE(s == "hello world");
}

/**
 * @test_case TC_CONTAINER_MAP_INSERT_LOOKUP
 * @tests REQ_PAL_CONTAINER_MAP
 */
TEST(ContainerTest, MapInsertLookup) {
    UnorderedMap<int, int, 8> m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    EXPECT_EQ(m.size(), 3U);
    EXPECT_NE(m.find(2), m.end());
    EXPECT_EQ(m.find(2)->second, 20);
    m.erase(2);
    EXPECT_EQ(m.size(), 2U);
    EXPECT_EQ(m.find(2), m.end());
}

/**
 * @test_case TC_CONTAINER_QUEUE_FIFO
 * @tests REQ_PAL_CONTAINER_QUEUE
 */
TEST(ContainerTest, QueueFIFO) {
    Queue<int, 4> q;
    EXPECT_TRUE(q.empty());
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.size(), 3U);
    EXPECT_EQ(q.front(), 1);
    q.pop();
    EXPECT_EQ(q.front(), 2);
    q.pop();
    EXPECT_EQ(q.front(), 3);
    q.pop();
    EXPECT_TRUE(q.empty());
}

/**
 * @test_case TC_CONTAINER_FUNCTION_INVOKE
 * @tests REQ_PAL_CONTAINER_FUNCTION
 */
TEST(ContainerTest, FunctionInvoke) {
    int captured = 42;
    Function<int(int), 32> fn = [captured](int x) { return captured + x; };
    EXPECT_EQ(fn(8), 50);
}

/**
 * @test_case TC_CONTAINER_CAPACITY_EXHAUST
 * @tests REQ_PAL_CONTAINER_CAPACITY_EXHAUST
 */
TEST(ContainerTest, CapacityExhaust) {
    Vector<int, 4> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.push_back(4);
    EXPECT_EQ(v.size(), 4U);
    EXPECT_TRUE(v.full());

    Queue<int, 2> q;
    q.push(1);
    q.push(2);
    EXPECT_EQ(q.size(), 2U);
    EXPECT_TRUE(q.full());
}

}  // namespace
}  // namespace someip::platform
