/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @brief Static pool allocator for SOME/IP Message objects on ThreadX.
 *
 * Uses a TX_BLOCK_POOL backed by a static buffer.  Pool creation is
 * performed lazily on first allocation, guarded by a TX_MUTEX.
 *
 * Pool size is configurable via SOMEIP_THREADX_MESSAGE_POOL_SIZE
 * (default 16).  When the pool is exhausted, allocate_message()
 * returns nullptr.
 */

#include "platform/memory.h"
#include "platform/threadx/memory_internal.h"

#include <tx_api.h>

#include <atomic>
#include <cstring>
#include <new>

#ifndef SOMEIP_THREADX_MESSAGE_POOL_SIZE
#define SOMEIP_THREADX_MESSAGE_POOL_SIZE 16
#endif

static constexpr size_t POOL_SIZE = SOMEIP_THREADX_MESSAGE_POOL_SIZE;

alignas(someip::Message) static UCHAR
    pool_buffer[POOL_SIZE * sizeof(someip::Message)];

TX_BLOCK_POOL message_pool;
static TX_MUTEX pool_guard;
std::atomic<bool> pool_initialized{false};

static void ensure_pool_init() {
    if (pool_initialized.load(std::memory_order_acquire)) {
        return;
    }

    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE

    if (!pool_initialized.load(std::memory_order_relaxed)) {
        UINT status = tx_mutex_create(&pool_guard,
                                      const_cast<CHAR*>("someip_pool_guard"),
                                      TX_NO_INHERIT);
        if (status == TX_SUCCESS) {
            status = tx_block_pool_create(&message_pool,
                                          const_cast<CHAR*>("someip_msg"),
                                          sizeof(someip::Message),
                                          pool_buffer,
                                          sizeof(pool_buffer));
            if (status != TX_SUCCESS) {
                tx_mutex_delete(&pool_guard);
            }
        }
        if (status == TX_SUCCESS) {
            pool_initialized.store(true, std::memory_order_release);
        }
    }

    TX_RESTORE
}

namespace someip::platform {

/** @implements REQ_PLATFORM_THREADX_002 */
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
    if (!msg) {
        return;
    }

    msg->~Message();
    tx_block_release(static_cast<void*>(msg));
}

}  // namespace someip::platform
