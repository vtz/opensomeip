/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @brief Static pool allocator for SOME/IP Message objects on FreeRTOS.
 * @implements REQ_PLATFORM_FREERTOS_002
 *
 * Uses a fixed-size static buffer with a bitmap to track free blocks.
 * All operations are protected by a FreeRTOS mutex.
 *
 * Pool size is configurable via SOMEIP_FREERTOS_MESSAGE_POOL_SIZE
 * (default 16).  When the pool is exhausted, allocate_message()
 * returns nullptr.
 */

#include "platform/memory.h"

#include <FreeRTOS.h>
#include <semphr.h>

#include <cstring>
#include <new>

#ifndef SOMEIP_FREERTOS_MESSAGE_POOL_SIZE
#define SOMEIP_FREERTOS_MESSAGE_POOL_SIZE 16
#endif

static constexpr size_t POOL_SIZE = SOMEIP_FREERTOS_MESSAGE_POOL_SIZE;

alignas(someip::Message) static char
    pool_buffer[POOL_SIZE * sizeof(someip::Message)];

static bool block_used[POOL_SIZE] = {};
static SemaphoreHandle_t pool_mutex = nullptr;
static bool pool_initialized = false;

static void ensure_pool_init() {
    if (pool_initialized) return;

    if (!pool_mutex) {
        pool_mutex = xSemaphoreCreateMutex();
        configASSERT(pool_mutex != nullptr);
    }

    xSemaphoreTake(pool_mutex, portMAX_DELAY);
    if (!pool_initialized) {
        std::memset(block_used, 0, sizeof(block_used));
        pool_initialized = true;
    }
    xSemaphoreGive(pool_mutex);
}

namespace someip {
namespace platform {

MessagePtr allocate_message() {
    ensure_pool_init();

    xSemaphoreTake(pool_mutex, portMAX_DELAY);

    for (size_t i = 0; i < POOL_SIZE; ++i) {
        if (!block_used[i]) {
            block_used[i] = true;
            xSemaphoreGive(pool_mutex);

            void* block = pool_buffer + i * sizeof(Message);
            auto* msg = new (block) Message();
            return MessagePtr(msg, [](Message* p) {
                release_message(p);
            });
        }
    }

    xSemaphoreGive(pool_mutex);
    return nullptr;
}

void release_message(Message* msg) {
    if (!msg) return;

    msg->~Message();

    auto* raw = reinterpret_cast<char*>(msg);
    size_t offset = static_cast<size_t>(raw - pool_buffer);
    size_t index = offset / sizeof(Message);

    if (index < POOL_SIZE) {
        xSemaphoreTake(pool_mutex, portMAX_DELAY);
        block_used[index] = false;
        xSemaphoreGive(pool_mutex);
    }
}

} // namespace platform
} // namespace someip
