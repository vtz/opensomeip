/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_FAKE_MEMORY_IMPL_H
#define SOMEIP_FAKE_MEMORY_IMPL_H

/**
 * @brief Fake memory allocator for protocol-layer unit tests.
 *
 * Records allocation counts so tests can verify protocol layers properly
 * allocate messages.
 */

#include <atomic>
#include <memory>

namespace someip {

class Message;
using MessagePtr = std::shared_ptr<Message>;

namespace platform {

inline std::atomic<int>& alloc_count() {
    static std::atomic<int> count{0};
    return count;
}

inline MessagePtr allocate_message() {
    ++alloc_count();
    return std::make_shared<Message>();
}

inline void reset_alloc_count() {
    alloc_count() = 0;
}

} // namespace platform
} // namespace someip

#endif // SOMEIP_FAKE_MEMORY_IMPL_H
