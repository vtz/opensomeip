/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <numeric>
#include <vector>

#include "platform/freertos/delay_ticks.h"
#include "platform/thread.h"

namespace {

class FreeRtosSleepChunks : public ::testing::Test {
   protected:
    void SetUp() override
    {
        mock_detail::delay_calls.clear();
    }
};

/**
 * @test_case TC_FREERTOS_SLEEP_CHUNKS
 * @tests REQ_PAL_SLEEP_DURATION
 */
TEST_F(FreeRtosSleepChunks, TickCapacityBoundariesPreserveTheFullDelay)
{
    constexpr uint64_t max_ticks = std::numeric_limits<TickType_t>::max();
    for (const uint64_t ticks : {max_ticks, max_ticks + 1, 2 * max_ticks + 17}) {
        mock_detail::delay_calls.clear();
        someip::platform::this_thread::sleep_for(
            std::chrono::milliseconds(ticks * 1000U / configTICK_RATE_HZ));
        EXPECT_EQ(std::accumulate(mock_detail::delay_calls.begin(), mock_detail::delay_calls.end(),
                                  uint64_t{0}),
                  ticks);
        ASSERT_EQ(mock_detail::delay_calls.size(), (ticks + max_ticks - 1) / max_ticks);
        uint64_t remaining = ticks;
        for (const auto chunk : mock_detail::delay_calls) {
            EXPECT_NE(chunk, 0);
            EXPECT_EQ(chunk, remaining < max_ticks ? remaining : max_ticks);
            remaining -= chunk;
        }
    }
}

TEST_F(FreeRtosSleepChunks, SixHundredFiftySixSecondsDoesNotWrap)
{
    someip::platform::this_thread::sleep_for(std::chrono::seconds(656));
    EXPECT_EQ(std::accumulate(mock_detail::delay_calls.begin(), mock_detail::delay_calls.end(),
                              uint64_t{0}),
              656U * configTICK_RATE_HZ);
#if configUSE_16_BIT_TICKS == 1 && configTICK_RATE_HZ == 100
    EXPECT_EQ(mock_detail::delay_calls, (std::vector<TickType_t>{65535, 65}));
#endif
}

/**
 * @test_case TC_FREERTOS_SLEEP_CHUNK_ROUNDING
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 */
TEST_F(FreeRtosSleepChunks, RoundsOnlyTheFinalFractionalTick)
{
    for (const uint64_t ms : {1U, 7U, 10U, 11U, 999U, 1000U, 1001U, 1999U}) {
        mock_detail::delay_calls.clear();
        someip::platform::this_thread::sleep_for(std::chrono::milliseconds(ms));
        ASSERT_EQ(mock_detail::delay_calls.size(), 1u);
        EXPECT_EQ(mock_detail::delay_calls.front(), (ms * configTICK_RATE_HZ + 999U) / 1000U);
    }
    mock_detail::delay_calls.clear();
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(0));
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(-1));
    someip::platform::this_thread::sleep_for(std::chrono::microseconds(-500));
    EXPECT_TRUE(mock_detail::delay_calls.empty());
    someip::platform::this_thread::sleep_for(std::chrono::microseconds(500));
    EXPECT_EQ(mock_detail::delay_calls, (std::vector<TickType_t>{1}));
}

/**
 * @test_case TC_FREERTOS_SLEEP_ARITHMETIC
 * @tests REQ_PAL_SLEEP_DURATION
 */
TEST(FreeRtosSleepArithmetic, BatchesBeforeMultiplicationOrAdditionCanOverflow)
{
    using someip::platform::detail::next_delay_batch;
    constexpr auto max_ticks = std::numeric_limits<uint64_t>::max();
    constexpr auto rate = std::numeric_limits<uint32_t>::max();
    constexpr auto seconds = max_ticks / rate;
    constexpr auto max_ms = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    const auto large = next_delay_batch(max_ms, rate);
    EXPECT_EQ(large.ticks, seconds * rate);
    EXPECT_EQ(large.remaining_ms, max_ms - seconds * 1000U);

    const auto exact = next_delay_batch(seconds * 1000U, rate);
    EXPECT_EQ(exact.ticks, max_ticks);
    EXPECT_EQ(exact.remaining_ms, 0u);
    const auto with_tail = next_delay_batch(seconds * 1000U + 999U, rate);
    EXPECT_EQ(with_tail.ticks, max_ticks);
    EXPECT_EQ(with_tail.remaining_ms, 999u);
    const auto tail = next_delay_batch(with_tail.remaining_ms, rate);
    EXPECT_EQ(tail.ticks, (uint64_t{999} * rate + 999U) / 1000U);
    EXPECT_EQ(tail.remaining_ms, 0u);
    EXPECT_EQ(next_delay_batch(0, rate).ticks, 0u);
}

}  // namespace
