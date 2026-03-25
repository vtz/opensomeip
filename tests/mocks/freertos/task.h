/*
 * Mock FreeRTOS task API for host-based PAL conformance testing.
 * Implements xTaskCreate via std::thread (detached) so PAL Thread tests work.
 */

#ifndef MOCK_FREERTOS_TASK_H
#define MOCK_FREERTOS_TASK_H

#include "FreeRTOS.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <new>

struct MockTaskHandle {};
typedef MockTaskHandle* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

namespace mock_detail {
inline MockTaskHandle task_sentinel;
} // namespace mock_detail

inline BaseType_t xTaskCreate(
        TaskFunction_t fn,
        const char* /*name*/,
        uint32_t    /*stack_depth*/,
        void*       param,
        UBaseType_t /*priority*/,
        TaskHandle_t* handle_out)
{
    std::thread t([fn, param]() { fn(param); });
    t.detach();

    if (handle_out) *handle_out = &mock_detail::task_sentinel;
    return pdPASS;
}

inline void vTaskDelete(TaskHandle_t /*handle*/) {
}

inline void vTaskDelay(TickType_t ticks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

inline TickType_t xTaskGetTickCount() {
    static auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<TickType_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

#endif /* MOCK_FREERTOS_TASK_H */
