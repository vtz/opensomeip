/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * FreeRTOS multi-task SOME/IP demo for Renode (STM32F407 Cortex-M4F).
 *
 * Demonstrates FreeRTOS task management with SOME/IP message processing:
 *
 *  - Producer task: creates SOME/IP messages with incrementing payloads,
 *    serializes them, and pushes the serialized bytes into a FreeRTOS queue.
 *
 *  - Consumer task: dequeues serialized messages, deserializes them, validates
 *    the round-trip integrity, and prints per-message results via UART.
 *
 *  - Uses std::atomic counters for thread-safe result tracking.
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

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

using namespace someip;

static const int NUM_MESSAGES = 10;
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

struct SerializedMsg {
    uint8_t data[256];
    size_t  len;
};

static QueueHandle_t msg_queue;

static void producer_task(void *) {
    printf("\n--- Producer: starting ---\n");

    for (int i = 0; i < NUM_MESSAGES; i++) {
        auto msg = someip::platform::allocate_message();
        if (!msg) {
            printf("  Producer: allocation failed at message %d\n", i);
            demo_failed.fetch_add(1);
            continue;
        }

        msg->set_service_id(0x1000 + i);
        msg->set_method_id(0x0001);
        std::vector<uint8_t> payload = {
            static_cast<uint8_t>(i),
            static_cast<uint8_t>(i + 1),
            static_cast<uint8_t>(i + 2),
            static_cast<uint8_t>(i + 3)
        };
        msg->set_payload(payload);

        auto serialized = msg->serialize();

        SerializedMsg smsg = {};
        size_t copy_len = serialized.size() < sizeof(smsg.data) ? serialized.size() : sizeof(smsg.data);
        memcpy(smsg.data, serialized.data(), copy_len);
        smsg.len = copy_len;

        if (xQueueSend(msg_queue, &smsg, pdMS_TO_TICKS(500)) != pdPASS) {
            printf("  Producer: queue full at message %d\n", i);
            demo_failed.fetch_add(1);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    printf("--- Producer: done (sent %d messages) ---\n", NUM_MESSAGES);
    vTaskDelete(NULL);
}

static void consumer_task(void *) {
    printf("\n--- Consumer: starting ---\n");

    int received = 0;
    SerializedMsg smsg;

    while (received < NUM_MESSAGES) {
        if (xQueueReceive(msg_queue, &smsg, pdMS_TO_TICKS(2000)) == pdPASS) {
            std::vector<uint8_t> data(smsg.data, smsg.data + smsg.len);

            Message decoded;
            bool ok = decoded.deserialize(data);

            char name_buf[64];
            snprintf(name_buf, sizeof(name_buf), "msg_%d_deserialize", received);
            DEMO_CHECK(ok, name_buf);

            if (ok) {
                snprintf(name_buf, sizeof(name_buf), "msg_%d_service_id_0x%04X", received,
                         0x1000 + received);
                DEMO_CHECK(decoded.get_service_id() == (0x1000 + received), name_buf);

                snprintf(name_buf, sizeof(name_buf), "msg_%d_payload_size", received);
                DEMO_CHECK(decoded.get_payload().size() == 4, name_buf);
            }

            received++;
        } else {
            printf("  Consumer: timeout waiting for message %d\n", received);
            demo_failed.fetch_add(1);
            break;
        }
    }

    printf("--- Consumer: done (received %d/%d messages) ---\n", received, NUM_MESSAGES);

    printf("\n=== Results: %d passed, %d failed ===\n",
           demo_passed.load(), demo_failed.load());

    exit(demo_failed.load() > 0 ? 1 : 0);
}

static void demo_task(void *) {
    printf("=== FreeRTOS Multi-Task SOME/IP Demo (Renode STM32F407) ===\n");

    msg_queue = xQueueCreate(NUM_MESSAGES, sizeof(SerializedMsg));
    if (!msg_queue) {
        printf("FATAL: xQueueCreate failed\n");
        exit(1);
    }

    if (xTaskCreate(producer_task, "producer", configMINIMAL_STACK_SIZE * 4,
                    NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        printf("FATAL: failed to create producer task\n");
        exit(1);
    }
    if (xTaskCreate(consumer_task, "consumer", configMINIMAL_STACK_SIZE * 4,
                    NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        printf("FATAL: failed to create consumer task\n");
        exit(1);
    }

    vTaskDelete(NULL);
}

int main() {
    if (xTaskCreate(demo_task, "demo", configMINIMAL_STACK_SIZE * 2,
                    NULL, tskIDLE_PRIORITY + 3, NULL) != pdPASS) {
        printf("FATAL: failed to create demo task\n");
        return 1;
    }
    vTaskStartScheduler();
    return 1;
}
