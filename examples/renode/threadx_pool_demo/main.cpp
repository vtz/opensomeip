/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * ThreadX memory pool SOME/IP demo for Renode (STM32F407 Cortex-M4).
 *
 * Demonstrates ThreadX thread management with SOME/IP message allocation:
 *
 *  - Allocator thread: Burst-allocates SOME/IP messages using
 *    platform::allocate_message() (ThreadX byte pool), serializes each,
 *    and stores the serialized data.
 *
 *  - Worker thread: Receives serialized messages, deserializes them,
 *    prints a hex dump of the first 16 bytes of each serialized message.
 *
 *  - Main thread: Orchestrates the demo and prints allocation statistics.
 *
 * Output goes to USART2 so Renode's FileTerminal can capture results.
 */

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "someip/message.h"
#include "someip/types.h"
#include "serialization/serializer.h"
#include "platform/memory.h"

extern "C" {
#include "tx_api.h"
}

using namespace someip;

static const int NUM_MESSAGES = 8;
static std::atomic<int> demo_passed{0};
static std::atomic<int> demo_failed{0};

#define DEMO_CHECK(cond, name)                                  \
    do {                                                        \
        if (cond) {                                             \
            printf("  [PASS] %s\n", name);                      \
            demo_passed.fetch_add(1);                           \
        } else {                                                \
            printf("  [FAIL] %s\n", name);                      \
            demo_failed.fetch_add(1);                           \
        }                                                       \
    } while (0)

struct MsgSlot {
    uint8_t data[256];
    size_t  len;
    bool    used;
};

static MsgSlot slots[NUM_MESSAGES];
static std::atomic<bool> allocator_done{false};
static std::atomic<int>  alloc_count{0};
static std::atomic<int>  release_count{0};

static void allocator_entry(ULONG) {
    printf("\n--- Allocator: burst-allocating %d messages ---\n", NUM_MESSAGES);

    for (int i = 0; i < NUM_MESSAGES; i++) {
        auto msg = someip::platform::allocate_message();

        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "alloc_msg_%d", i);
        DEMO_CHECK(msg != nullptr, name_buf);

        if (!msg) continue;
        alloc_count.fetch_add(1);

        msg->set_service_id(0x2000 + i);
        msg->set_method_id(0x0010);
        std::vector<uint8_t> payload;
        for (int j = 0; j < 8; j++) {
            payload.push_back(static_cast<uint8_t>((i * 8) + j));
        }
        msg->set_payload(payload);

        auto serialized = msg->serialize();
        size_t copy_len = serialized.size() < sizeof(slots[i].data)
                              ? serialized.size()
                              : sizeof(slots[i].data);
        memcpy(slots[i].data, serialized.data(), copy_len);
        slots[i].len = copy_len;
        slots[i].used = true;

        msg.reset();
        release_count.fetch_add(1);
    }

    printf("--- Allocator: done (allocated=%d, released=%d) ---\n",
           alloc_count.load(), release_count.load());
    allocator_done.store(true);
}

static void worker_entry(ULONG) {
    while (!allocator_done.load()) {
        tx_thread_sleep(10);
    }

    printf("\n--- Worker: processing %d messages ---\n", NUM_MESSAGES);

    for (int i = 0; i < NUM_MESSAGES; i++) {
        if (!slots[i].used) continue;

        std::vector<uint8_t> data(slots[i].data, slots[i].data + slots[i].len);

        Message decoded;
        bool ok = decoded.deserialize(data);

        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "deser_msg_%d", i);
        DEMO_CHECK(ok, name_buf);

        if (ok) {
            snprintf(name_buf, sizeof(name_buf), "msg_%d_svc_0x%04X", i, 0x2000 + i);
            DEMO_CHECK(decoded.get_service_id() == (0x2000 + i), name_buf);

            snprintf(name_buf, sizeof(name_buf), "msg_%d_payload_8B", i);
            DEMO_CHECK(decoded.get_payload().size() == 8, name_buf);
        }

        printf("  msg[%d] hex: ", i);
        int dump_len = slots[i].len < 16 ? (int)slots[i].len : 16;
        for (int j = 0; j < dump_len; j++) {
            printf("%02X ", slots[i].data[j]);
        }
        printf("\n");
    }

    printf("--- Worker: done ---\n");

    printf("\n--- Pool stats: allocated=%d released=%d ---\n",
           alloc_count.load(), release_count.load());

    printf("\n=== Results: %d passed, %d failed ===\n",
           demo_passed.load(), demo_failed.load());

    exit(demo_failed.load() > 0 ? 1 : 0);
}

static TX_THREAD alloc_thread;
static TX_THREAD work_thread;
static UCHAR alloc_stack[4096];
static UCHAR work_stack[4096];

extern "C" void tx_application_define(void *) {
    tx_thread_create(&alloc_thread,
                     const_cast<CHAR *>("allocator"),
                     allocator_entry, 0,
                     alloc_stack, sizeof(alloc_stack),
                     2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_thread_create(&work_thread,
                     const_cast<CHAR *>("worker"),
                     worker_entry, 0,
                     work_stack, sizeof(work_stack),
                     3, 3, TX_NO_TIME_SLICE, TX_AUTO_START);
}

int main() {
    printf("=== ThreadX Pool Demo: SOME/IP Message Allocation (Renode STM32F407) ===\n");
    tx_kernel_enter();
    return 0;
}
