/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_THREAD_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_THREAD_IMPL_H

/**
 * @brief Zephyr threading backend.
 */

#include <zephyr/kernel.h>
#include <functional>
#include <chrono>
#include <cstring>

#ifndef CONFIG_SOMEIP_THREAD_STACK_SIZE
#define CONFIG_SOMEIP_THREAD_STACK_SIZE 4096
#endif

namespace someip {
namespace platform {

/** @implements REQ_PLATFORM_ZEPHYR_001, REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_MUTEX_NONCOPY, REQ_PAL_MUTEX_UNLOCK_E01 */
class Mutex {
public:
    Mutex()  { k_mutex_init(&m_); }
    ~Mutex() = default;

    /** @implements REQ_PAL_MUTEX_LOCK */
    void lock()   {
        int rc = k_mutex_lock(&m_, K_FOREVER);
        __ASSERT(rc == 0, "k_mutex_lock failed: %d", rc);
        (void)rc;
    }
    /** @implements REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_UNLOCK_E01 */
    void unlock() { k_mutex_unlock(&m_); }
    /** @implements REQ_PAL_MUTEX_TRYLOCK */
    bool try_lock() { return k_mutex_lock(&m_, K_NO_WAIT) == 0; }

    k_mutex* native_handle() { return &m_; }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
private:
    k_mutex m_;
};

/** @implements REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP */
class ConditionVariable {
public:
    ConditionVariable()  { k_condvar_init(&cv_); }
    ~ConditionVariable() = default;

    /** @implements REQ_PAL_CV_NOTIFY_ONE */
    void notify_one() { k_condvar_signal(&cv_); }
    /** @implements REQ_PAL_CV_NOTIFY_ALL */
    void notify_all() { k_condvar_broadcast(&cv_); }

    /** @implements REQ_PAL_CV_WAIT, REQ_PAL_CV_OWNERSHIP */
    void wait(Mutex& mtx) {
        k_condvar_wait(&cv_, mtx.native_handle(), K_FOREVER);
    }

    /** @implements REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_OWNERSHIP */
    template <typename Pred>
    void wait(Mutex& mtx, Pred pred) {
        while (!pred()) {
            k_condvar_wait(&cv_, mtx.native_handle(), K_FOREVER);
        }
    }

    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;
private:
    k_condvar cv_;
};

/** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_THREAD_NONCOPY, REQ_PAL_THREAD_CREATE_E01, REQ_PAL_THREAD_DTOR_E01 */
class Thread {
public:
    Thread() = default;

    /** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_CREATE_E01 */
    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args) {
        ctx_ = new std::function<void()>(
            [f = std::forward<Fn>(fn),
             a = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply(std::move(f), std::move(a));
            });
        k_thread_create(&thread_, stack_,
                        K_THREAD_STACK_SIZEOF(stack_),
                        trampoline, ctx_, nullptr, nullptr,
                        K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
        started_ = true;
    }

    /** @implements REQ_PAL_THREAD_DTOR_E01 */
    ~Thread() {
        if (joinable()) {
            k_thread_abort(&thread_);
            delete ctx_;
            ctx_ = nullptr;
            started_ = false;
        }
    }

    /** @implements REQ_PAL_THREAD_JOINABLE */
    bool joinable() const { return started_ && !joined_; }

    /** Returns true if the thread was successfully created and started. */
    bool started() const { return started_; }

    /** @implements REQ_PAL_THREAD_JOIN */
    void join() {
        if (joinable()) {
            k_thread_join(&thread_, K_FOREVER);
            joined_ = true;
            delete ctx_;
            ctx_ = nullptr;
        }
    }

    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

private:
    static void trampoline(void* p1, void*, void*) {
        auto* fn = static_cast<std::function<void()>*>(p1);
        if (fn && *fn) (*fn)();
    }

    k_thread thread_{};
    K_KERNEL_STACK_MEMBER(stack_, CONFIG_SOMEIP_THREAD_STACK_SIZE);
    std::function<void()>* ctx_{nullptr};
    bool started_{false};
    bool joined_{false};
};

namespace this_thread {

/** @implements REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO */
template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& d) {
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    if (ms <= 0) return;
    while (ms > 0) {
        int32_t chunk = (ms > INT32_MAX) ? INT32_MAX : static_cast<int32_t>(ms);
        k_msleep(chunk);
        ms -= chunk;
    }
}

} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_ZEPHYR_THREAD_IMPL_H
