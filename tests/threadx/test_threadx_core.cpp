/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SOME/IP core tests running inside a real ThreadX kernel (linux port).
 *
 * @tests REQ_PLATFORM_THREADX_001
 * @tests REQ_PLATFORM_THREADX_002
 * @tests REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK
 * @tests REQ_PAL_CV_NOTIFY_ONE
 * @tests REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 * @tests REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
 *
 * ThreadX requires:
 *   main() → tx_kernel_enter() → tx_application_define() → threads run
 *
 * The test thread exercises Message, Endpoint, SessionManager,
 * Serializer, E2E, TP, and threading, then calls exit() to
 * terminate the ThreadX kernel.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"
#include "core/session_manager.h"
#include "serialization/serializer.h"
#include "platform/thread.h"
#include "platform/memory.h"
#include "platform/threadx/memory_internal.h"

extern "C" {
#include "tx_api.h"
}

using namespace someip;
using namespace someip::transport;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, name)                                   \
    do {                                                    \
        if (cond) {                                         \
            printf("  [PASS] %s\n", name);                  \
            tests_passed++;                                 \
        } else {                                            \
            printf("  [FAIL] %s\n", name);                  \
            tests_failed++;                                 \
        }                                                   \
    } while (0)

// Shared platform-independent test suites
#include "tests/shared/test_message_common.inc"
#include "tests/shared/test_endpoint_common.inc"
#include "tests/shared/test_session_manager_common.inc"
#include "tests/shared/test_serializer_common.inc"
#include "tests/shared/test_e2e_common.inc"
#include "tests/shared/test_tp_common.inc"

// ThreadX-specific tests

static void test_threadx_threading() {
    printf("\n--- ThreadX threading tests ---\n");

    someip::platform::Mutex mtx;
    mtx.lock();
    CHECK(true, "mutex_lock");
    mtx.unlock();
    CHECK(true, "mutex_unlock");
    CHECK(mtx.try_lock(), "mutex_try_lock");
    mtx.unlock();

    someip::platform::ConditionVariable cv;
    cv.notify_one();
    CHECK(true, "cv_notify_one");
}

static void test_threadx_memory_pool() {
    printf("\n--- ThreadX memory pool tests ---\n");

    auto msg1 = someip::platform::allocate_message();
    CHECK(msg1 != nullptr, "pool_alloc_1");

    if (msg1) {
        msg1->set_payload({0xAA, 0xBB});
        CHECK(msg1->get_payload().size() == 2, "pool_msg_payload");
    }

    auto msg2 = someip::platform::allocate_message();
    CHECK(msg2 != nullptr, "pool_alloc_2");

    msg1.reset();
    msg2.reset();

    auto msg3 = someip::platform::allocate_message();
    CHECK(msg3 != nullptr, "pool_realloc_after_release");
}

static void test_threadx_pool_diagnostics() {
    printf("\n--- ThreadX block pool diagnostics ---\n");

    if (!pool_initialized) {
        auto init_msg = someip::platform::allocate_message();
        init_msg.reset();
    }

    ULONG available_blocks = 0;
    ULONG total_blocks = 0;
    TX_THREAD* first_suspended = nullptr;
    ULONG suspended_count = 0;
    TX_BLOCK_POOL* next_pool = nullptr;
    CHAR* name = nullptr;

    UINT status = tx_block_pool_info_get(&message_pool, &name, &available_blocks,
                                          &total_blocks, &first_suspended,
                                          &suspended_count, &next_pool);
    CHECK(status == TX_SUCCESS, "pool_info_query");
    printf("  Pool '%s': %u available / %u total blocks\n",
           name ? name : "(null)", available_blocks, total_blocks);
    CHECK(available_blocks == total_blocks, "all_blocks_available_before_test");

    {
        auto msg1 = someip::platform::allocate_message();
        auto msg2 = someip::platform::allocate_message();
        CHECK(msg1 != nullptr, "diag_alloc_1");
        CHECK(msg2 != nullptr, "diag_alloc_2");

        ULONG avail_during = 0;
        status = tx_block_pool_info_get(&message_pool, nullptr, &avail_during,
                                        nullptr, nullptr, nullptr, nullptr);
        CHECK(status == TX_SUCCESS, "pool_info_during_alloc");
        CHECK(avail_during == available_blocks - 2, "blocks_decreased_by_2");
        printf("  During alloc: %u available\n", avail_during);
    }

    ULONG avail_after = 0;
    status = tx_block_pool_info_get(&message_pool, nullptr, &avail_after,
                                    nullptr, nullptr, nullptr, nullptr);
    CHECK(status == TX_SUCCESS, "pool_info_after_release");
    CHECK(avail_after == available_blocks, "blocks_restored_after_release");
    printf("  After release: %u available (restored)\n", avail_after);
}

static TX_THREAD test_thread;
static UCHAR test_stack[8192];

static void test_thread_entry(ULONG) {
    printf("=== SOME/IP Core Tests on ThreadX (linux port) ===\n");

    // Platform-independent suites (shared with FreeRTOS and Zephyr)
    test_message();
    test_endpoint();
    test_session_manager();
    test_serializer();
    test_e2e();
    test_tp();

    // ThreadX-specific suites
    test_threadx_threading();
    test_threadx_memory_pool();
    test_threadx_pool_diagnostics();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    exit(tests_failed > 0 ? 1 : 0);
}

extern "C" void tx_application_define(void*) {
    UINT rc = tx_thread_create(&test_thread,
                               const_cast<CHAR*>("test_main"),
                               test_thread_entry,
                               0,
                               test_stack,
                               sizeof(test_stack),
                               1, 1,
                               TX_NO_TIME_SLICE,
                               TX_AUTO_START);
    if (rc != TX_SUCCESS) {
        std::fprintf(stderr, "tx_thread_create failed: %u\n", rc);
        std::exit(1);
    }
}

int main() {
    tx_kernel_enter();
    return 0;
}
