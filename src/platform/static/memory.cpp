/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
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

/**
 * @implements REQ_PLATFORM_STATIC_002, REQ_PAL_MEM_ALLOC,
 *             REQ_PAL_MEM_EXHAUST_E01, REQ_PAL_MEM_THREADSAFE_E01
 */

#include "static_config.h"
#include "etl_error_handler.h"
#include "platform/memory.h"
#include "platform/thread.h"
#include "platform/intrusive_ptr.h"
#include "someip/message.h"

#include <cstdint>
#include <new>

namespace someip::platform {

namespace {

alignas(alignof(Message)) static uint8_t
    message_slab[SOMEIP_MESSAGE_POOL_SIZE][sizeof(Message)];

static uint16_t free_stack[SOMEIP_MESSAGE_POOL_SIZE];
static bool     in_use[SOMEIP_MESSAGE_POOL_SIZE];
static uint16_t stack_top{0};

static Mutex pool_mutex;

}  // namespace

void init_static_allocator() {
    register_etl_error_handler();
    init_buffer_pool();
    init_message_pool();
}

void init_message_pool() {
    ScopedLock lk(pool_mutex);
    for (uint16_t i = 0; i < SOMEIP_MESSAGE_POOL_SIZE; ++i) {
        free_stack[i] = i;
        in_use[i] = false;
    }
    stack_top = SOMEIP_MESSAGE_POOL_SIZE;
}

MessagePtr allocate_message() {
    ScopedLock lk(pool_mutex);

    if (stack_top == 0) {
        return MessagePtr{};
    }
    --stack_top;
    uint16_t idx = free_stack[stack_top];
    in_use[idx] = true;

    auto* msg = new (&message_slab[idx][0]) Message();
    return MessagePtr(msg, true);
}

void release_message(Message* msg) {
    if (!msg) { return; }

    auto* raw = reinterpret_cast<uint8_t*>(msg);
    auto* base = &message_slab[0][0];
    auto* end  = &message_slab[SOMEIP_MESSAGE_POOL_SIZE - 1][0] + sizeof(Message);

    if (raw < base || raw >= end) { return; }

    size_t offset = static_cast<size_t>(raw - base);
    if (offset % sizeof(Message) != 0) { return; }
    auto idx = static_cast<uint16_t>(offset / sizeof(Message));

    ScopedLock lk(pool_mutex);
    if (!in_use[idx]) { return; }

    msg->~Message();

    in_use[idx] = false;
    if (stack_top < SOMEIP_MESSAGE_POOL_SIZE) {
        free_stack[stack_top] = idx;
        ++stack_top;
    }
}

}  // namespace someip::platform
