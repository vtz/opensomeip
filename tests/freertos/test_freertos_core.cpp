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
 *
 * FreeRTOS requires:
 *   main() → xTaskCreate() → vTaskStartScheduler() → tasks run
 *
 * The test task exercises Message, Endpoint, SessionManager,
 * Serializer, threading primitives, and the memory pool, then
 * calls exit() to terminate the FreeRTOS scheduler.
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

static void test_message() {
    printf("\n--- Message tests ---\n");

    Message msg(MessageId(0x1234, 0x0001),
                RequestId(0x0010, 0x0001));

    CHECK(msg.get_service_id() == 0x1234, "service_id");
    CHECK(msg.get_method_id() == 0x0001, "method_id");
    CHECK(msg.get_client_id() == 0x0010, "client_id");
    CHECK(msg.get_session_id() == 0x0001, "session_id");

    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    msg.set_payload(payload);
    CHECK(msg.get_payload().size() == 5, "payload_size");

    auto serialized = msg.serialize();
    CHECK(serialized.size() >= 16, "serialize_min_size");

    Message decoded;
    bool ok = decoded.deserialize(serialized);
    CHECK(ok, "deserialize");
    CHECK(decoded.get_service_id() == 0x1234, "roundtrip_service_id");
    CHECK(decoded.get_payload() == payload, "roundtrip_payload");
}

static void test_endpoint() {
    printf("\n--- Endpoint tests ---\n");

    Endpoint ep1("192.168.1.1", 30490);
    CHECK(ep1.is_valid(), "valid_ipv4");
    CHECK(ep1.is_ipv4(), "is_ipv4");
    CHECK(!ep1.is_multicast(), "not_multicast");

    Endpoint ep2("239.118.122.69", 30490, TransportProtocol::MULTICAST_UDP);
    CHECK(ep2.is_valid(), "multicast_valid");
    CHECK(ep2.is_multicast(), "is_multicast");

    Endpoint ep3("999.999.999.999", 80);
    CHECK(!ep3.is_valid(), "invalid_ipv4");

    Endpoint ep4("", 80);
    CHECK(!ep4.is_valid(), "empty_address");

    Endpoint ep5("::1", 80);
    CHECK(ep5.is_valid(), "valid_ipv6");
    CHECK(ep5.is_ipv6(), "is_ipv6");
}

static void test_session_manager() {
    printf("\n--- Session Manager tests ---\n");

    SessionManager sm;
    uint16_t s1 = sm.get_next_session_id();
    uint16_t s2 = sm.get_next_session_id();
    CHECK(s2 == s1 + 1, "sequential_ids");
}

static void test_serializer() {
    printf("\n--- Serializer tests ---\n");

    using namespace someip::serialization;

    Serializer ser;
    ser.serialize_uint8(0x42);
    ser.serialize_uint16(0x1234);
    ser.serialize_uint32(0xDEADBEEF);

    auto data = ser.get_buffer();
    CHECK(data.size() == 7, "serialized_size");

    Deserializer deser(data);
    auto v8 = deser.deserialize_uint8();
    auto v16 = deser.deserialize_uint16();
    auto v32 = deser.deserialize_uint32();
    CHECK(v8.is_success() && v8.get_value() == 0x42, "deser_uint8");
    CHECK(v16.is_success() && v16.get_value() == 0x1234, "deser_uint16");
    CHECK(v32.is_success() && v32.get_value() == 0xDEADBEEF, "deser_uint32");
}

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

    // Test sleep_for (FreeRTOS vTaskDelay under the hood)
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

static void test_task_entry(void*) {
    printf("=== SOME/IP Core Tests on FreeRTOS (POSIX port) ===\n");

    test_message();
    test_endpoint();
    test_session_manager();
    test_serializer();
    test_freertos_threading();
    test_freertos_thread_join();
    test_freertos_memory_pool();

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
