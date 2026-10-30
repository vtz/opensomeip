/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <gtest/gtest.h>

#include "platform/thread.h"

/**
 * @test_case TC_FREERTOS_SLEEP_SUBTICK
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 */
TEST(FreeRtosSleep, PositiveSubTickSleepBlocksForAtLeastOneTick)
{
    static_assert(configTICK_RATE_HZ == 100);
    static_assert(pdMS_TO_TICKS(1) == 0);
    mock_detail::last_delay_ticks = portMAX_DELAY;
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), 1u);
    mock_detail::last_delay_ticks = portMAX_DELAY;
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(0));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), portMAX_DELAY);
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(-1));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), portMAX_DELAY);
}

/**
 * @test_case TC_FREERTOS_SLEEP_CEILING
 * @tests REQ_PAL_SLEEP_DURATION
 */
TEST(FreeRtosSleep, FractionalTicksRoundUpWithoutExtendingExactTicks)
{
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), 1u);
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(11));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), 2u);
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), 2u);
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(21));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), 3u);
}

/**
 * @test_case TC_FREERTOS_SLEEP_FRACTIONAL_MILLISECOND
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 */
TEST(FreeRtosSleep, PositiveSubMillisecondDurationsRequestOneTick)
{
    mock_detail::last_delay_ticks = portMAX_DELAY;
    someip::platform::this_thread::sleep_for(std::chrono::microseconds(500));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), 1u);
    mock_detail::last_delay_ticks = portMAX_DELAY;
    someip::platform::this_thread::sleep_for(std::chrono::microseconds(-500));
    EXPECT_EQ(mock_detail::last_delay_ticks.load(), portMAX_DELAY);
}
