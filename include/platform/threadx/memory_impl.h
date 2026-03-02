/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_THREADX_MEMORY_IMPL_H
#define SOMEIP_PLATFORM_THREADX_MEMORY_IMPL_H

/**
 * @brief ThreadX memory pool backend.
 * @implements REQ_PLATFORM_THREADX_002
 *
 * Provides a static pool allocator for Message objects using a
 * ThreadX TX_BLOCK_POOL.
 *
 * Pool size: SOMEIP_THREADX_MESSAGE_POOL_SIZE (default 16).
 * Exhaustion: returns nullptr (callers must check).
 */

namespace someip {
namespace platform {

MessagePtr allocate_message();
void release_message(Message* msg);

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_THREADX_MEMORY_IMPL_H
