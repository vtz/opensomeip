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

/**
 * @test_case TC_BUFPOOL_ACQUIRE_SMALL, TC_BUFPOOL_RELEASE_REUSE,
 *            TC_BUFPOOL_TIER_SELECT, TC_BUFPOOL_EXHAUST,
 *            TC_BUFPOOL_TIERED_ALLOC, TC_BUFPOOL_CONCURRENT,
 *            TC_STATIC_MSG_POOL_ALLOC, TC_INTRUSIVE_PTR_LIFETIME,
 *            TC_BYTEBUFFER_API, TC_PIMPL_NO_HEAP,
 *            TC_CONTAINER_VECTOR_PUSH_BACK, TC_CONTAINER_STRING_APPEND,
 *            TC_CONTAINER_MAP_INSERT_LOOKUP, TC_CONTAINER_QUEUE_FIFO,
 *            TC_CONTAINER_FUNCTION_INVOKE, TC_CONTAINER_CAPACITY_EXHAUST
 * @tests REQ_PAL_BUFPOOL_ACQUIRE, REQ_PAL_BUFPOOL_RELEASE,
 *        REQ_PAL_BUFPOOL_TIERED, REQ_PAL_BUFPOOL_EXHAUST_E01,
 *        REQ_PLATFORM_STATIC_002, REQ_PLATFORM_STATIC_003,
 *        REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_EXHAUST_E01,
 *        REQ_PAL_INTRUSIVE_PTR,
 *        REQ_PAL_CONTAINER_VECTOR, REQ_PAL_CONTAINER_STRING,
 *        REQ_PAL_CONTAINER_MAP, REQ_PAL_CONTAINER_QUEUE,
 *        REQ_PAL_CONTAINER_FUNCTION, REQ_PAL_CONTAINER_CAPACITY_EXHAUST
 */

#include <gtest/gtest.h>

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/intrusive_ptr.h"
#include "platform/memory.h"
#include "platform/thread.h"
#include "someip/message.h"
#include "static_config.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace someip::platform {
namespace {

// --- ByteBuffer tests ---

class ByteBufferTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(ByteBufferTest, DefaultConstructEmpty) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_EQ(buf.data(), nullptr);
}

TEST_F(ByteBufferTest, InitializerListConstruct) {
    ByteBuffer buf{0x01, 0x02, 0x03};
    ASSERT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(buf[2], 0x03);
}

TEST_F(ByteBufferTest, SizeValueConstruct) {
    ByteBuffer buf(10, 0xAA);
    ASSERT_EQ(buf.size(), 10u);
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(buf[i], 0xAA);
    }
}

TEST_F(ByteBufferTest, PushBack) {
    ByteBuffer buf;
    buf.push_back(0x42);
    buf.push_back(0x43);
    ASSERT_EQ(buf.size(), 2u);
    EXPECT_EQ(buf[0], 0x42);
    EXPECT_EQ(buf[1], 0x43);
}

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

TEST_F(ByteBufferTest, ResizeWithValue) {
    ByteBuffer buf;
    buf.resize(4, 0xFF);
    ASSERT_EQ(buf.size(), 4u);
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(buf[i], 0xFF);
    }
}

TEST_F(ByteBufferTest, Clear) {
    ByteBuffer buf{0x01, 0x02, 0x03};
    buf.clear();
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_TRUE(buf.empty());
    EXPECT_GE(buf.capacity(), 3u);
}

TEST_F(ByteBufferTest, Reserve) {
    ByteBuffer buf;
    buf.reserve(100);
    EXPECT_GE(buf.capacity(), 100u);
    EXPECT_EQ(buf.size(), 0u);
}

TEST_F(ByteBufferTest, MoveConstruct) {
    ByteBuffer a{0x01, 0x02, 0x03};
    ByteBuffer b(std::move(a));
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_TRUE(a.empty()); // NOLINT(bugprone-use-after-move)
}

TEST_F(ByteBufferTest, MoveAssign) {
    ByteBuffer a{0x01, 0x02};
    ByteBuffer b{0x03, 0x04, 0x05};
    b = std::move(a);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x01);
}

TEST_F(ByteBufferTest, CopyConstruct) {
    ByteBuffer a{0x01, 0x02, 0x03};
    ByteBuffer b(a);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_EQ(b[2], 0x03);

    b[0] = 0xFF;
    EXPECT_EQ(a[0], 0x01);
}

TEST_F(ByteBufferTest, CopyAssign) {
    ByteBuffer a{0x01, 0x02};
    ByteBuffer b;
    b = a;
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[1], 0x02);
}

TEST_F(ByteBufferTest, IteratorRange) {
    ByteBuffer buf{0x10, 0x20, 0x30};
    std::vector<uint8_t> vec(buf.begin(), buf.end());
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 0x10);
    EXPECT_EQ(vec[2], 0x30);
}

TEST_F(ByteBufferTest, Equality) {
    ByteBuffer a{0x01, 0x02};
    ByteBuffer b{0x01, 0x02};
    ByteBuffer c{0x01, 0x03};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

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

// --- Buffer pool tier selection tests ---

class BufferPoolTest : public ::testing::Test {};

TEST_F(BufferPoolTest, AcquireSmall) {
    BufferSlot* s = acquire_buffer(100);
    ASSERT_NE(s, nullptr);
    EXPECT_GE(s->capacity, 100u);
    EXPECT_EQ(s->tier, 0u);
    release_buffer(s);
}

TEST_F(BufferPoolTest, AcquireMedium) {
    BufferSlot* s = acquire_buffer(SOMEIP_BYTE_POOL_SMALL_SIZE + 1);
    ASSERT_NE(s, nullptr);
    EXPECT_GE(s->capacity, SOMEIP_BYTE_POOL_SMALL_SIZE + 1);
    EXPECT_EQ(s->tier, 1u);
    release_buffer(s);
}

TEST_F(BufferPoolTest, AcquireLarge) {
    BufferSlot* s = acquire_buffer(SOMEIP_BYTE_POOL_MEDIUM_SIZE + 1);
    ASSERT_NE(s, nullptr);
    EXPECT_GE(s->capacity, SOMEIP_BYTE_POOL_MEDIUM_SIZE + 1);
    EXPECT_EQ(s->tier, 2u);
    release_buffer(s);
}

TEST_F(BufferPoolTest, ReleaseAndReuse) {
    BufferSlot* a = acquire_buffer(10);
    ASSERT_NE(a, nullptr);
    release_buffer(a);

    BufferSlot* b = acquire_buffer(10);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a, b);
    release_buffer(b);
}

TEST_F(BufferPoolTest, ExhaustSmallTier) {
    std::vector<BufferSlot*> slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_SMALL_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(1);
        ASSERT_NE(s, nullptr);
        slots.push_back(s);
    }

    // Next small acquire should fall back to medium tier
    BufferSlot* fallback = acquire_buffer(1);
    if (fallback) {
        EXPECT_GE(fallback->tier, 1u);
        release_buffer(fallback);
    }

    for (auto* s : slots) {
        release_buffer(s);
    }
}

TEST_F(BufferPoolTest, ExhaustAllTiers) {
    std::vector<BufferSlot*> all_slots;
    size_t total = SOMEIP_BYTE_POOL_SMALL_COUNT +
                   SOMEIP_BYTE_POOL_MEDIUM_COUNT +
                   SOMEIP_BYTE_POOL_LARGE_COUNT;

    for (size_t i = 0; i < total; ++i) {
        BufferSlot* s = acquire_buffer(1);
        if (s) {
            all_slots.push_back(s);
        }
    }

    BufferSlot* exhausted = acquire_buffer(1);
    EXPECT_EQ(exhausted, nullptr);

    for (auto* s : all_slots) {
        release_buffer(s);
    }
}

TEST_F(BufferPoolTest, NullReleaseIsSafe) {
    release_buffer(nullptr);
}

TEST_F(BufferPoolTest, WriteToSlot) {
    BufferSlot* s = acquire_buffer(64);
    ASSERT_NE(s, nullptr);
    std::memset(s->data, 0xAB, 64);
    EXPECT_EQ(s->data[0], 0xAB);
    EXPECT_EQ(s->data[63], 0xAB);
    release_buffer(s);
}

// --- Message pool tests ---

class MessagePoolTest : public ::testing::Test {};

TEST_F(MessagePoolTest, AllocateReturnsValid) {
    MessagePtr msg = allocate_message();
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->get_protocol_version(), 0x01);
}

TEST_F(MessagePoolTest, AllocateMultiple) {
    std::vector<MessagePtr> msgs;
    for (int i = 0; i < 5; ++i) {
        auto m = allocate_message();
        ASSERT_TRUE(m) << "Failed at allocation " << i;
        msgs.push_back(std::move(m));
    }
    msgs.clear();
}

TEST_F(MessagePoolTest, ExhaustPool) {
    std::vector<MessagePtr> msgs;
    for (int i = 0; i < SOMEIP_MESSAGE_POOL_SIZE; ++i) {
        auto m = allocate_message();
        ASSERT_TRUE(m) << "Failed at allocation " << i;
        msgs.push_back(std::move(m));
    }

    auto exhausted = allocate_message();
    EXPECT_FALSE(exhausted);

    msgs.clear();

    auto recycled = allocate_message();
    EXPECT_TRUE(recycled);
}

TEST_F(MessagePoolTest, ReleaseAndRealloc) {
    auto a = allocate_message();
    ASSERT_TRUE(a);
    a.reset();

    auto b = allocate_message();
    EXPECT_TRUE(b);
}

// --- IntrusivePtr tests ---

class IntrusivePtrTest : public ::testing::Test {};

TEST_F(IntrusivePtrTest, DefaultNull) {
    IntrusivePtr<Message> p;
    EXPECT_FALSE(p);
    EXPECT_EQ(p.get(), nullptr);
}

TEST_F(IntrusivePtrTest, NullptrConstruct) {
    IntrusivePtr<Message> p = nullptr;
    EXPECT_FALSE(p);
}

TEST_F(IntrusivePtrTest, MessageLifecycle) {
    auto msg = allocate_message();
    ASSERT_TRUE(msg);
    msg->set_service_id(0x1234);

    {
        auto copy = msg;
        EXPECT_TRUE(copy);
        EXPECT_EQ(copy->get_service_id(), 0x1234);
        EXPECT_EQ(msg.get(), copy.get());
    }

    EXPECT_TRUE(msg);
    EXPECT_EQ(msg->get_service_id(), 0x1234);
}

TEST_F(IntrusivePtrTest, MoveTransfersOwnership) {
    auto a = allocate_message();
    ASSERT_TRUE(a);
    Message* raw = a.get();

    IntrusivePtr<Message> b(std::move(a));
    EXPECT_FALSE(a); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(b.get(), raw);
}

TEST_F(IntrusivePtrTest, Reset) {
    auto msg = allocate_message();
    ASSERT_TRUE(msg);
    msg.reset();
    EXPECT_FALSE(msg);
}

TEST_F(IntrusivePtrTest, Comparison) {
    auto a = allocate_message();
    auto b = allocate_message();
    EXPECT_NE(a, b);

    auto c = a;
    EXPECT_EQ(a, c);

    IntrusivePtr<Message> n;
    EXPECT_EQ(n, nullptr);
    EXPECT_NE(a, nullptr);
}

// --- Concurrent tests ---

TEST(ConcurrentTest, BufferPoolThreadSafety) {
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 50;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < kOpsPerThread; ++i) {
            BufferSlot* s = acquire_buffer(64);
            if (s) {
                s->data[0] = 0x42;
                release_buffer(s);
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

TEST(ConcurrentTest, MessagePoolThreadSafety) {
    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 4;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < kOpsPerThread; ++i) {
            auto msg = allocate_message();
            if (msg) {
                msg->set_service_id(0xBEEF);
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
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
