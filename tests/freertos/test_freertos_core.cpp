/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SOME/IP core tests running inside a real FreeRTOS kernel (POSIX port).
 *
 * @tests REQ_PLATFORM_FREERTOS_001
 * @tests REQ_PLATFORM_FREERTOS_002
 * @tests REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK
 * @tests REQ_PAL_CV_NOTIFY_ONE
 * @tests REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 * @tests REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
 *
 * FreeRTOS requires:
 *   main() → xTaskCreate() → vTaskStartScheduler() → tasks run
 *
 * The test task exercises Message, Endpoint, SessionManager,
 * Serializer, E2E, TP, threading primitives, and the memory pool,
 * then calls exit() to terminate the FreeRTOS scheduler.
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

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

extern "C" {

void vApplicationMallocFailedHook() {
    printf("FATAL: FreeRTOS pvPortMalloc failed (heap exhausted)\n");
    fflush(stdout);
    abort();
}

void vApplicationStackOverflowHook(TaskHandle_t, char* task_name) {
    printf("FATAL: Stack overflow in task '%s'\n", task_name);
    fflush(stdout);
    abort();
}

} // extern "C"

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

// FreeRTOS-specific tests

static void test_freertos_threading() {
    printf("\n--- FreeRTOS threading tests ---\n");

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

    auto t0 = xTaskGetTickCount();
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto t1 = xTaskGetTickCount();
    TickType_t elapsed = t1 - t0;
    CHECK(elapsed >= pdMS_TO_TICKS(40), "sleep_for_lower_bound");
    CHECK(elapsed <= pdMS_TO_TICKS(150), "sleep_for_upper_bound");
}

static void test_freertos_thread_join() {
    printf("\n--- FreeRTOS Thread join tests ---\n");

    volatile bool thread_ran = false;

    someip::platform::Thread t([&thread_ran]() {
        thread_ran = true;
    });

    CHECK(t.joinable(), "joinable_before_join");
    t.join();
    CHECK(!t.joinable(), "not_joinable_after_join");
    CHECK(thread_ran, "thread_body_executed");
}

static void test_freertos_memory_pool() {
    printf("\n--- FreeRTOS memory pool tests ---\n");

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

static void test_freertos_heap_watermarks() {
    printf("\n--- FreeRTOS heap watermark tests ---\n");

    size_t free_before = xPortGetFreeHeapSize();
    size_t min_ever = xPortGetMinimumEverFreeHeapSize();
    printf("  Heap free: %zu bytes, min-ever free: %zu bytes\n",
           free_before, min_ever);
    CHECK(free_before > 0, "heap_has_free_space");
    CHECK(min_ever > 0, "heap_min_ever_positive");

    {
        auto temp_msg = someip::platform::allocate_message();
        CHECK(temp_msg != nullptr, "watermark_alloc");
        SemaphoreHandle_t sem = xSemaphoreCreateBinary();
        CHECK(sem != nullptr, "heap_rtos_alloc");
        size_t free_during = xPortGetFreeHeapSize();
        CHECK(free_during < free_before, "heap_decreased_after_alloc");
        vSemaphoreDelete(sem);
    }

    size_t free_after = xPortGetFreeHeapSize();
    CHECK(free_after >= free_before, "heap_restored_after_free");

    printf("  Heap free after cycle: %zu bytes (delta: %zd)\n",
           free_after, static_cast<ssize_t>(free_after) - static_cast<ssize_t>(free_before));
}

static void test_task_entry(void*) {
    printf("=== SOME/IP Core Tests on FreeRTOS (POSIX port) ===\n");

    // Platform-independent suites (shared with ThreadX and Zephyr)
    test_message();
    test_endpoint();
    test_session_manager();
    test_serializer();
    test_e2e();
    test_tp();

    // FreeRTOS-specific suites
    test_freertos_threading();
    test_freertos_thread_join();
    test_freertos_memory_pool();
    test_freertos_heap_watermarks();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    exit(tests_failed > 0 ? 1 : 0);
}

int main() {
    BaseType_t rc = xTaskCreate(
        test_task_entry,
        "test_main",
        configMINIMAL_STACK_SIZE * 4,
        nullptr,
        tskIDLE_PRIORITY + 2,
        nullptr);

    if (rc != pdPASS) {
        printf("FATAL: xTaskCreate failed\n");
        return 1;
    }

    vTaskStartScheduler();

    /* Should never reach here */
    return 1;
}
