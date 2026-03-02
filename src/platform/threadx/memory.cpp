/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @brief Static pool allocator for SOME/IP Message objects on ThreadX.
 * @implements REQ_PLATFORM_THREADX_002
 *
 * Uses a TX_BLOCK_POOL backed by a static buffer.  Pool creation is
 * performed lazily on first allocation, guarded by a TX_MUTEX.
 *
 * Pool size is configurable via SOMEIP_THREADX_MESSAGE_POOL_SIZE
 * (default 16).  When the pool is exhausted, allocate_message()
 * returns nullptr.
 */

#include "platform/memory.h"

#include <tx_api.h>

#include <cstring>
#include <new>

#ifndef SOMEIP_THREADX_MESSAGE_POOL_SIZE
#define SOMEIP_THREADX_MESSAGE_POOL_SIZE 16
#endif

static constexpr size_t POOL_SIZE = SOMEIP_THREADX_MESSAGE_POOL_SIZE;

alignas(someip::Message) static UCHAR
    pool_buffer[POOL_SIZE * sizeof(someip::Message)];

static TX_BLOCK_POOL message_pool;
static TX_MUTEX pool_guard;
static bool pool_initialized = false;

static void ensure_pool_init() {
    if (pool_initialized) return;

    static bool guard_created = false;
    if (!guard_created) {
        tx_mutex_create(&pool_guard, const_cast<CHAR*>("someip_pool_guard"),
                        TX_NO_INHERIT);
        guard_created = true;
    }

    tx_mutex_get(&pool_guard, TX_WAIT_FOREVER);
    if (!pool_initialized) {
        tx_block_pool_create(&message_pool,
                             const_cast<CHAR*>("someip_msg"),
                             sizeof(someip::Message),
                             pool_buffer,
                             sizeof(pool_buffer));
        pool_initialized = true;
    }
    tx_mutex_put(&pool_guard);
}

namespace someip {
namespace platform {

MessagePtr allocate_message() {
    ensure_pool_init();

    void* block = nullptr;
    UINT status = tx_block_allocate(&message_pool, &block, TX_NO_WAIT);
    if (status != TX_SUCCESS) {
        return nullptr;
    }

    auto* msg = new (block) Message();
    return MessagePtr(msg, [](Message* p) {
        release_message(p);
    });
}

void release_message(Message* msg) {
    if (!msg) return;

    msg->~Message();
    tx_block_release(static_cast<void*>(msg));
}

} // namespace platform
} // namespace someip
