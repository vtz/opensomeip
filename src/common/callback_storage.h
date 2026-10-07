/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
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

#ifndef SOMEIP_COMMON_CALLBACK_STORAGE_H
#define SOMEIP_COMMON_CALLBACK_STORAGE_H

#include <optional>
#include <utility>

#include "platform/thread.h"

namespace someip::detail {

/**
 * @brief Release completed-session callbacks without copying a fixed-capacity map.
 * @implements REQ_ARCH_002, REQ_ARCH_003
 * @note Mutators must be quiesced. Move/copy and moved-from capture destruction
 *       may occur under the mutex; the extracted value is destroyed after unlocking.
 */
template <typename Map>
// NOLINTNEXTLINE(misc-include-cleaner) - platform::Mutex from platform/thread.h dispatch header
void release_entries(Map& entries, platform::Mutex& mutex)
{
    typename Map::size_type remaining = 0;
    {
        platform::ScopedLock const lock(mutex);
        remaining = entries.size();
    }
    while (remaining > 0) {
        --remaining;
        std::optional<typename Map::mapped_type> released;
        {
            platform::ScopedLock const lock(mutex);
            if (entries.empty()) {
                return;
            }
            auto entry = entries.begin();
            released.emplace(std::move(entry->second));
            entries.erase(entry);
        }
    }
}

}  // namespace someip::detail

#endif  // SOMEIP_COMMON_CALLBACK_STORAGE_H
