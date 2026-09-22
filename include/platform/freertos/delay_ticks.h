/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_FREERTOS_DELAY_TICKS_H
#define SOMEIP_PLATFORM_FREERTOS_DELAY_TICKS_H

#include <cstdint>
#include <limits>

namespace someip::platform::detail {

struct DelayBatch {
    uint64_t ticks;
    uint64_t remaining_ms;
};

// Whole-second batches are exact; round only the final sub-second remainder.
// A positive rate is supplied by the FreeRTOS configuration.
constexpr DelayBatch next_delay_batch(uint64_t milliseconds, uint32_t rate)
{
    const uint64_t seconds = milliseconds / 1000U;
    const uint64_t limit = std::numeric_limits<uint64_t>::max() / rate;
    if (seconds != 0) {
        const uint64_t batch_seconds = seconds < limit ? seconds : limit;
        const uint64_t ticks = batch_seconds * rate;
        const uint64_t remainder = milliseconds - batch_seconds * 1000U;
        if (remainder < 1000U) {
            const uint64_t tail = (remainder * rate + 999U) / 1000U;
            if (tail <= std::numeric_limits<uint64_t>::max() - ticks) {
                return {ticks + tail, 0};
            }
        }
        return {ticks, remainder};
    }
    return {(milliseconds * rate + 999U) / 1000U, 0};
}

}  // namespace someip::platform::detail

#endif  // SOMEIP_PLATFORM_FREERTOS_DELAY_TICKS_H
