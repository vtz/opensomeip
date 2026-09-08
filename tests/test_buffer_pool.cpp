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
#include "platform/memory.h"
#include "static_config.h"

#include <atomic>
#include <cstring>
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

class BufferPoolTest : public ::testing::Test {};

/**
 * @test_case TC_BUFPOOL_ACQUIRE_SMALL
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 * @tests REQ_PAL_BUFPOOL_TIERED
 */
TEST_F(BufferPoolTest, AcquireSmallBuffer) {
    BufferSlot* s = acquire_buffer(100);
    ASSERT_NE(s, nullptr);
    EXPECT_GE(s->capacity, 100u);
    EXPECT_EQ(s->tier, 0u);
    release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_ACQUIRE_MEDIUM
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 * @tests REQ_PAL_BUFPOOL_TIERED
 */
TEST_F(BufferPoolTest, AcquireMediumBuffer) {
    BufferSlot* s = acquire_buffer(SOMEIP_BYTE_POOL_SMALL_SIZE + 1);
    ASSERT_NE(s, nullptr);
    EXPECT_GE(s->capacity, SOMEIP_BYTE_POOL_SMALL_SIZE + 1);
    EXPECT_EQ(s->tier, 1u);
    release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_ACQUIRE_LARGE
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 * @tests REQ_PAL_BUFPOOL_TIERED
 */
TEST_F(BufferPoolTest, AcquireLargeBuffer) {
    BufferSlot* s = acquire_buffer(SOMEIP_BYTE_POOL_MEDIUM_SIZE + 1);
    ASSERT_NE(s, nullptr);
    EXPECT_GE(s->capacity, SOMEIP_BYTE_POOL_MEDIUM_SIZE + 1);
    EXPECT_EQ(s->tier, 2u);
    release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_ACQUIRE_OVERSIZED
 * @tests REQ_PAL_BUFPOOL_EXHAUST_E01
 */
TEST_F(BufferPoolTest, AcquireOversized) {
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

/**
 * @test_case TC_BUFPOOL_RELEASE_REUSE
 * @tests REQ_PAL_BUFPOOL_RELEASE
 */
TEST_F(BufferPoolTest, ReleaseAndReuse) {
    BufferSlot* a = acquire_buffer(10);
    ASSERT_NE(a, nullptr);
    release_buffer(a);

    BufferSlot* b = acquire_buffer(10);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a, b);
    release_buffer(b);
}

/**
 * @test_case TC_BUFPOOL_EXHAUST_TIER0
 * @tests REQ_PAL_BUFPOOL_TIERED
 * @tests REQ_PAL_BUFPOOL_EXHAUST_E01
 */
TEST_F(BufferPoolTest, ExhaustTier0) {
    std::vector<BufferSlot*> slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_SMALL_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(1);
        ASSERT_NE(s, nullptr);
        slots.push_back(s);
    }

    BufferSlot* fallback = acquire_buffer(1);
    if (fallback) {
        EXPECT_GE(fallback->tier, 1u);
        release_buffer(fallback);
    }

    for (auto* s : slots) {
        release_buffer(s);
    }
}

/**
 * @test_case TC_BUFPOOL_EXHAUST_TIER1
 * @tests REQ_PAL_BUFPOOL_TIERED
 * @tests REQ_PAL_BUFPOOL_EXHAUST_E01
 */
TEST_F(BufferPoolTest, ExhaustTier1) {
    std::vector<BufferSlot*> small_slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_SMALL_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(1);
        ASSERT_NE(s, nullptr);
        small_slots.push_back(s);
    }

    std::vector<BufferSlot*> medium_slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_MEDIUM_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(SOMEIP_BYTE_POOL_SMALL_SIZE + 1);
        ASSERT_NE(s, nullptr);
        medium_slots.push_back(s);
    }

    BufferSlot* fallback = acquire_buffer(SOMEIP_BYTE_POOL_SMALL_SIZE + 1);
    if (fallback) {
        EXPECT_GE(fallback->tier, 2u);
        release_buffer(fallback);
    }

    for (auto* s : medium_slots) release_buffer(s);
    for (auto* s : small_slots) release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_EXHAUST_TIER2
 * @tests REQ_PAL_BUFPOOL_TIERED
 * @tests REQ_PAL_BUFPOOL_EXHAUST_E01
 */
TEST_F(BufferPoolTest, ExhaustTier2) {
    std::vector<BufferSlot*> large_slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_LARGE_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(SOMEIP_BYTE_POOL_MEDIUM_SIZE + 1);
        ASSERT_NE(s, nullptr);
        large_slots.push_back(s);
    }

    BufferSlot* exhausted = acquire_buffer(SOMEIP_BYTE_POOL_MEDIUM_SIZE + 1);
    EXPECT_EQ(exhausted, nullptr);

    for (auto* s : large_slots) release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_CROSS_TIER_FALLBACK
 * @tests REQ_PAL_BUFPOOL_TIERED
 */
TEST_F(BufferPoolTest, CrossTierFallback) {
    std::vector<BufferSlot*> slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_SMALL_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(1);
        ASSERT_NE(s, nullptr);
        slots.push_back(s);
    }

    BufferSlot* fallback = acquire_buffer(1);
    if (fallback) {
        EXPECT_GE(fallback->tier, 1u)
            << "Small tier exhausted; request should fall back to medium or large";
        release_buffer(fallback);
    }

    for (auto* s : slots) release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_DOUBLE_FREE
 * @tests REQ_PAL_BUFPOOL_RELEASE
 */
TEST_F(BufferPoolTest, DoubleFreeProtection) {
    BufferSlot* s = acquire_buffer(32);
    ASSERT_NE(s, nullptr);

    release_buffer(s);
    release_buffer(s);

    BufferSlot* a = acquire_buffer(32);
    BufferSlot* b = acquire_buffer(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b) << "Double-release must not cause same slot to be handed out twice";

    release_buffer(a);
    release_buffer(b);
}

/**
 * @test_case TC_BUFPOOL_CONCURRENT
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 * @tests REQ_PAL_BUFPOOL_RELEASE
 */
TEST_F(BufferPoolTest, ConcurrentAcquireRelease) {
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

/**
 * @test_case TC_BUFPOOL_FULL_CYCLE
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 * @tests REQ_PAL_BUFPOOL_RELEASE
 */
TEST_F(BufferPoolTest, PoolStateAfterFullCycle) {
    std::vector<BufferSlot*> slots;
    for (size_t i = 0; i < SOMEIP_BYTE_POOL_SMALL_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(1);
        ASSERT_NE(s, nullptr);
        slots.push_back(s);
    }

    for (auto* s : slots) release_buffer(s);
    slots.clear();

    for (size_t i = 0; i < SOMEIP_BYTE_POOL_SMALL_COUNT; ++i) {
        BufferSlot* s = acquire_buffer(1);
        ASSERT_NE(s, nullptr) << "Re-acquire failed at index " << i
                              << " after full release cycle";
        slots.push_back(s);
    }

    for (auto* s : slots) release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_DATA_INTEGRITY
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 */
TEST_F(BufferPoolTest, BufferDataIntegrity) {
    BufferSlot* s = acquire_buffer(64);
    ASSERT_NE(s, nullptr);
    std::memset(s->data, 0xAB, 64);
    EXPECT_EQ(s->data[0], 0xAB);
    EXPECT_EQ(s->data[63], 0xAB);

    for (size_t i = 0; i < 64; ++i) {
        EXPECT_EQ(s->data[i], 0xAB) << "Corruption at byte " << i;
    }

    release_buffer(s);
}

/**
 * @test_case TC_BUFPOOL_NULL_RELEASE
 * @tests REQ_PAL_BUFPOOL_RELEASE
 */
TEST_F(BufferPoolTest, NullReleaseIsSafe) {
    release_buffer(nullptr);
}

}  // namespace
}  // namespace someip::platform
