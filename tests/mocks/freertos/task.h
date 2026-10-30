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
#ifdef SOMEIP_FREERTOS_CAPTURE_DELAYS
#include <vector>
#endif

struct MockTaskHandle {};
typedef MockTaskHandle* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

namespace mock_detail {
inline MockTaskHandle task_sentinel;
inline std::atomic<TickType_t> last_delay_ticks{portMAX_DELAY};
#ifdef SOMEIP_FREERTOS_CAPTURE_DELAYS
inline std::vector<TickType_t> delay_calls;
#endif
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
    mock_detail::last_delay_ticks = ticks;
#ifdef SOMEIP_FREERTOS_CAPTURE_DELAYS
    mock_detail::delay_calls.push_back(ticks);
#else
    std::this_thread::sleep_for(
        std::chrono::milliseconds((static_cast<uint64_t>(ticks) * 1000) / configTICK_RATE_HZ));
#endif
}

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    // Distinct per host thread: each std::thread that calls this gets its own
    // thread_local instance, so the returned address is a stable per-thread id
    // (unlike task_sentinel above, which is shared by every created task).
    thread_local MockTaskHandle self_sentinel;
    return &self_sentinel;
}

inline TickType_t xTaskGetTickCount() {
    static auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<TickType_t>(
        (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() *
         configTICK_RATE_HZ) /
        1000);
}

#endif /* MOCK_FREERTOS_TASK_H */
