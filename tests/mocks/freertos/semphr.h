/*
 * Mock FreeRTOS semaphore API for host-based PAL conformance testing.
 * Implements mutex, binary, and counting semaphores using std primitives.
 */

#ifndef MOCK_FREERTOS_SEMPHR_H
#define MOCK_FREERTOS_SEMPHR_H

#include "FreeRTOS.h"
#include <mutex>
#include <condition_variable>

struct MockSemaphore {
    enum Type { MUTEX, BINARY, COUNTING };
    Type type;
    std::mutex mtx;
    std::condition_variable cv;
    int count;
    int max_count;

    MockSemaphore(Type t, int initial, int max_c)
        : type(t), count(initial), max_count(max_c) {}
};

typedef MockSemaphore* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    return new MockSemaphore(MockSemaphore::MUTEX, 1, 1);
}

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return new MockSemaphore(MockSemaphore::BINARY, 0, 1);
}

inline SemaphoreHandle_t xSemaphoreCreateCounting(
        UBaseType_t max_count, UBaseType_t initial) {
    return new MockSemaphore(MockSemaphore::COUNTING,
                             static_cast<int>(initial),
                             static_cast<int>(max_count));
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) {
    if (!sem) return pdFAIL;
    std::unique_lock<std::mutex> lk(sem->mtx);
    if (timeout == 0) {
        if (sem->count > 0) { --sem->count; return pdTRUE; }
        return pdFALSE;
    }
    auto deadline = std::chrono::milliseconds(static_cast<unsigned long>(timeout));
    if (!sem->cv.wait_for(lk, deadline, [&] { return sem->count > 0; })) {
        return pdFALSE;
    }
    --sem->count;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
    if (!sem) return pdFAIL;
    std::lock_guard<std::mutex> lk(sem->mtx);
    if (sem->count < sem->max_count) {
        ++sem->count;
        sem->cv.notify_one();
        return pdTRUE;
    }
    return pdFAIL;
}

inline void vSemaphoreDelete(SemaphoreHandle_t sem) {
    delete sem;
}

#endif /* MOCK_FREERTOS_SEMPHR_H */
