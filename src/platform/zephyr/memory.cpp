/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/*
 * k_mem_slab-based pool allocator for SOME/IP Message objects.
 * Compiled only for embedded Zephyr targets (not native_sim).
 */

#if defined(__ZEPHYR__) && !defined(CONFIG_ARCH_POSIX)

#include "platform/memory.h"
#include <zephyr/kernel.h>

#ifndef CONFIG_SOMEIP_MESSAGE_POOL_SIZE
#define CONFIG_SOMEIP_MESSAGE_POOL_SIZE 16
#endif

static char message_pool_buffer[CONFIG_SOMEIP_MESSAGE_POOL_SIZE * sizeof(someip::Message)]
    __aligned(8);
static struct k_mem_slab message_slab;
static bool slab_initialized = false;
static struct k_mutex slab_init_mutex;

static void ensure_slab_init() {
    if (slab_initialized) return;
    k_mutex_lock(&slab_init_mutex, K_FOREVER);
    if (!slab_initialized) {
        k_mem_slab_init(&message_slab, message_pool_buffer,
                        sizeof(someip::Message),
                        CONFIG_SOMEIP_MESSAGE_POOL_SIZE);
        slab_initialized = true;
    }
    k_mutex_unlock(&slab_init_mutex);
}

static int slab_init_mutex_setup(void) {
    k_mutex_init(&slab_init_mutex);
    return 0;
}
SYS_INIT(slab_init_mutex_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

namespace someip {
namespace platform {

MessagePtr allocate_message() {
    ensure_slab_init();

    void* block = nullptr;
    if (k_mem_slab_alloc(&message_slab, &block, K_NO_WAIT) != 0) {
        return nullptr;
    }

    auto* msg = new (block) Message();
    return MessagePtr(msg, [](Message* p) {
        release_message(p);
    });
}

void release_message(Message* msg) {
    if (msg) {
        msg->~Message();
        k_mem_slab_free(&message_slab, msg);
    }
}

} // namespace platform
} // namespace someip

#endif
