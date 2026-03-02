/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_FREERTOS_THREAD_IMPL_H
#define SOMEIP_PLATFORM_FREERTOS_THREAD_IMPL_H

/**
 * @brief FreeRTOS threading backend.
 * @implements REQ_PLATFORM_FREERTOS_001
 *
 * Provides Mutex, ConditionVariable, Thread, and sleep_for using
 * FreeRTOS primitives.
 *
 * Required FreeRTOS configuration:
 *   configUSE_MUTEXES                 1
 *   configUSE_COUNTING_SEMAPHORES     1
 *   configSUPPORT_DYNAMIC_ALLOCATION  1
 *
 * Thread lifecycle:
 *   - Construction: xTaskCreate() with a trampoline that runs the
 *     user-supplied callable.  A binary semaphore is created for join().
 *   - join(): blocks on the binary semaphore (given by the trampoline
 *     after the user function returns).  Idempotent.
 *   - Destructor: if the thread is still joinable, aborts the task
 *     via vTaskDelete() and cleans up resources.
 *
 * ConditionVariable semantics:
 *   Uses a counting semaphore.  notify_one() gives one token;
 *   wait() consumes one token.  Callers must always use the predicate
 *   form to handle stale tokens (spurious wakeups are impossible with
 *   FreeRTOS semaphores, but the predicate loop is required anyway
 *   because a token may have been given before the waiter entered wait).
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <functional>
#include <chrono>
#include <cstdint>
#include <tuple>

#ifndef SOMEIP_FREERTOS_THREAD_STACK_SIZE
#define SOMEIP_FREERTOS_THREAD_STACK_SIZE 4096
#endif

#ifndef SOMEIP_FREERTOS_THREAD_PRIORITY
#define SOMEIP_FREERTOS_THREAD_PRIORITY (tskIDLE_PRIORITY + 2)
#endif

namespace someip {
namespace platform {

class Mutex {
public:
    Mutex() : handle_(xSemaphoreCreateMutex()) {
        configASSERT(handle_ != nullptr);
    }

    ~Mutex() {
        if (handle_) vSemaphoreDelete(handle_);
    }

    void lock() {
        configASSERT(handle_ != nullptr);
        xSemaphoreTake(handle_, portMAX_DELAY);
    }

    void unlock() {
        configASSERT(handle_ != nullptr);
        xSemaphoreGive(handle_);
    }

    bool try_lock() {
        return xSemaphoreTake(handle_, 0) == pdTRUE;
    }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

private:
    SemaphoreHandle_t handle_;
};

class ConditionVariable {
public:
    ConditionVariable()
        : sem_(xSemaphoreCreateCounting(
              configQUEUE_REGISTRY_SIZE > 0 ? configQUEUE_REGISTRY_SIZE : 8,
              0)) {
        configASSERT(sem_ != nullptr);
    }

    ~ConditionVariable() {
        if (sem_) vSemaphoreDelete(sem_);
    }

    void notify_one() { xSemaphoreGive(sem_); }

    void notify_all() {
        /* Best-effort: give multiple tokens.  Current codebase only uses
           notify_one() in production; notify_all() is provided for API
           completeness. */
        for (int i = 0; i < 4; ++i) {
            xSemaphoreGive(sem_);
        }
    }

    void wait(Mutex& mtx) {
        mtx.unlock();
        xSemaphoreTake(sem_, portMAX_DELAY);
        mtx.lock();
    }

    template <typename Pred>
    void wait(Mutex& mtx, Pred pred) {
        while (!pred()) {
            wait(mtx);
        }
    }

    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

private:
    SemaphoreHandle_t sem_;
};

class Thread {
public:
    Thread() = default;

    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args) {
        join_sem_ = xSemaphoreCreateBinary();
        configASSERT(join_sem_ != nullptr);

        ctx_ = new std::function<void()>(
            [f = std::forward<Fn>(fn),
             a = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply(std::move(f), std::move(a));
            });

        BaseType_t rc = xTaskCreate(
            trampoline,
            "someip",
            SOMEIP_FREERTOS_THREAD_STACK_SIZE / sizeof(StackType_t),
            this,
            SOMEIP_FREERTOS_THREAD_PRIORITY,
            &task_handle_);

        if (rc != pdPASS) {
            delete ctx_;
            ctx_ = nullptr;
            vSemaphoreDelete(join_sem_);
            join_sem_ = nullptr;
            return;
        }
        started_ = true;
    }

    ~Thread() {
        if (joinable()) {
            if (task_handle_) vTaskDelete(task_handle_);
            delete ctx_;
            ctx_ = nullptr;
            if (join_sem_) vSemaphoreDelete(join_sem_);
            join_sem_ = nullptr;
            started_ = false;
        }
    }

    bool joinable() const { return started_ && !joined_; }

    void join() {
        if (!joinable()) return;
        xSemaphoreTake(join_sem_, portMAX_DELAY);
        joined_ = true;
        delete ctx_;
        ctx_ = nullptr;
        vSemaphoreDelete(join_sem_);
        join_sem_ = nullptr;
        task_handle_ = nullptr;
    }

    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

private:
    static void trampoline(void* param) {
        auto* self = static_cast<Thread*>(param);
        if (self->ctx_ && *(self->ctx_)) {
            (*(self->ctx_))();
        }
        if (self->join_sem_) {
            xSemaphoreGive(self->join_sem_);
        }
        vTaskDelete(nullptr);
    }

    TaskHandle_t task_handle_{nullptr};
    SemaphoreHandle_t join_sem_{nullptr};
    std::function<void()>* ctx_{nullptr};
    bool started_{false};
    bool joined_{false};
};

namespace this_thread {

template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& d) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    if (ms <= 0) return;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_FREERTOS_THREAD_IMPL_H
