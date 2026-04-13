/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @brief Static pool allocator for SOME/IP Message objects on FreeRTOS.
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
#include <task.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>

#ifndef SOMEIP_FREERTOS_MESSAGE_POOL_SIZE
#define SOMEIP_FREERTOS_MESSAGE_POOL_SIZE 16
#endif

static constexpr size_t POOL_SIZE = SOMEIP_FREERTOS_MESSAGE_POOL_SIZE;

namespace {

alignas(someip::Message) char
    pool_buffer[POOL_SIZE * sizeof(someip::Message)];

bool block_used[POOL_SIZE] = {};
SemaphoreHandle_t pool_mutex = nullptr;
std::atomic<bool> pool_initialized{false};

void ensure_pool_init() {
    if (pool_initialized.load(std::memory_order_acquire)) {
        return;
    }

    taskENTER_CRITICAL();

    if (!pool_initialized.load(std::memory_order_relaxed)) {
        pool_mutex = xSemaphoreCreateMutex();
        configASSERT(pool_mutex != nullptr);
        if (pool_mutex != nullptr) {
            std::memset(block_used, 0, sizeof(block_used));
            pool_initialized.store(true, std::memory_order_release);
        }
    }

    taskEXIT_CRITICAL();
}

void release_message_impl(someip::Message* msg) {
    if (!msg) {
        return;
    }

    auto raw_addr = reinterpret_cast<std::uintptr_t>(msg);
    auto pool_addr = reinterpret_cast<std::uintptr_t>(pool_buffer);
    if (raw_addr < pool_addr || raw_addr >= pool_addr + POOL_SIZE * sizeof(someip::Message)) {
        return;
    }
    size_t const byte_offset = raw_addr - pool_addr;
    if (byte_offset % sizeof(someip::Message) != 0) {
        return;
    }
    size_t const index = byte_offset / sizeof(someip::Message);

    msg->~Message();

    xSemaphoreTake(pool_mutex, portMAX_DELAY);
    block_used[index] = false;
    xSemaphoreGive(pool_mutex);
}

}  // namespace

namespace someip::platform {

/** @implements REQ_PLATFORM_FREERTOS_002 */
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
    release_message_impl(msg);
}

}  // namespace someip::platform
