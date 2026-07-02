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

#include "platform/intrusive_ptr.h"
#include "platform/memory.h"
#include "someip/message.h"
#include "static_config.h"

#include <atomic>
#include <thread>
#include <vector>

namespace someip::platform {
namespace {

class StaticAllocEnv : public ::testing::Environment {
public:
    void SetUp() override { init_static_allocator(); }
};
[[maybe_unused]] auto* const g_env =
    ::testing::AddGlobalTestEnvironment(new StaticAllocEnv());

// --- Message pool tests ---

class MessagePoolTest : public ::testing::Test {};

/**
 * @test_case TC_MSGPOOL_ALLOC_USABLE
 * @tests REQ_PAL_MEM_ALLOC
 */
TEST_F(MessagePoolTest, AllocateReturnsUsable) {
    MessagePtr msg = allocate_message();
    ASSERT_TRUE(msg);
    EXPECT_EQ(msg->get_protocol_version(), 0x01);
}

/**
 * @test_case TC_MSGPOOL_INDEPENDENT
 * @tests REQ_PAL_MEM_ALLOC
 */
TEST_F(MessagePoolTest, IndependentAllocations) {
    std::vector<MessagePtr> msgs;
    for (int i = 0; i < 5; ++i) {
        auto m = allocate_message();
        ASSERT_TRUE(m) << "Failed at allocation " << i;
        msgs.push_back(std::move(m));
    }
    msgs.clear();
}

/**
 * @test_case TC_MSGPOOL_INTRUSIVE_PTR
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
TEST_F(MessagePoolTest, IntrusivePtrReturnsToPool) {
    auto a = allocate_message();
    ASSERT_TRUE(a);
    a.reset();

    auto b = allocate_message();
    EXPECT_TRUE(b);
}

/**
 * @test_case TC_MSGPOOL_EXHAUST
 * @tests REQ_PAL_MEM_EXHAUST_E01
 */
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

/**
 * @test_case TC_MSGPOOL_EXHAUST_RECOVER
 * @tests REQ_PAL_MEM_ALLOC
 * @tests REQ_PAL_MEM_EXHAUST_E01
 */
TEST_F(MessagePoolTest, ExhaustAndRecover) {
    std::vector<MessagePtr> msgs;
    for (int i = 0; i < SOMEIP_MESSAGE_POOL_SIZE; ++i) {
        auto m = allocate_message();
        ASSERT_TRUE(m) << "Failed at allocation " << i;
        msgs.push_back(std::move(m));
    }

    EXPECT_FALSE(allocate_message());

    msgs.erase(msgs.begin());

    auto recovered = allocate_message();
    EXPECT_TRUE(recovered) << "Pool should recover after releasing one slot";
}

/**
 * @test_case TC_MSGPOOL_PLACEMENT_NEW
 * @tests REQ_PAL_MEM_ALLOC
 * @tests REQ_PLATFORM_STATIC_002
 */
TEST_F(MessagePoolTest, PlacementNewInitialization) {
    auto msg = allocate_message();
    ASSERT_TRUE(msg);

    msg->set_service_id(0xAAAA);
    msg->set_method_id(0xBBBB);
    EXPECT_EQ(msg->get_service_id(), 0xAAAA);
    EXPECT_EQ(msg->get_method_id(), 0xBBBB);

    msg.reset();

    auto fresh = allocate_message();
    ASSERT_TRUE(fresh);
    EXPECT_EQ(fresh->get_protocol_version(), 0x01)
        << "Re-allocated message should be freshly constructed";
}

/**
 * @test_case TC_MSGPOOL_CONCURRENT
 * @tests REQ_PAL_MEM_ALLOC
 */
TEST_F(MessagePoolTest, ConcurrentAllocRelease) {
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

// --- IntrusivePtr tests ---

class IntrusivePtrTest : public ::testing::Test {};

/**
 * @test_case TC_INTRUSIVE_DEFAULT_NULL
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
TEST_F(IntrusivePtrTest, DefaultNull) {
    IntrusivePtr<Message> p;
    EXPECT_FALSE(p);
    EXPECT_EQ(p.get(), nullptr);
}

/**
 * @test_case TC_INTRUSIVE_NULLPTR_CONSTRUCT
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
TEST_F(IntrusivePtrTest, NullptrConstruct) {
    IntrusivePtr<Message> p = nullptr;
    EXPECT_FALSE(p);
}

/**
 * @test_case TC_INTRUSIVE_LIFECYCLE
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
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

/**
 * @test_case TC_INTRUSIVE_MOVE
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
TEST_F(IntrusivePtrTest, MoveTransfersOwnership) {
    auto a = allocate_message();
    ASSERT_TRUE(a);
    Message* raw = a.get();

    IntrusivePtr<Message> b(std::move(a));
    EXPECT_FALSE(a); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(b.get(), raw);
}

/**
 * @test_case TC_INTRUSIVE_RESET
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
TEST_F(IntrusivePtrTest, Reset) {
    auto msg = allocate_message();
    ASSERT_TRUE(msg);
    msg.reset();
    EXPECT_FALSE(msg);
}

/**
 * @test_case TC_INTRUSIVE_COMPARISON
 * @tests REQ_PAL_INTRUSIVE_PTR
 */
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

}  // namespace
}  // namespace someip::platform
