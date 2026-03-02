/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_THREAD_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_THREAD_IMPL_H

#include <zephyr/kernel.h>
#include <functional>
#include <chrono>
#include <cstring>

#ifndef CONFIG_SOMEIP_THREAD_STACK_SIZE
#define CONFIG_SOMEIP_THREAD_STACK_SIZE 4096
#endif

namespace someip {
namespace platform {

class Mutex {
public:
    Mutex()  { k_mutex_init(&m_); }
    ~Mutex() = default;

    void lock()   {
        int rc = k_mutex_lock(&m_, K_FOREVER);
        __ASSERT(rc == 0, "k_mutex_lock failed: %d", rc);
        (void)rc;
    }
    void unlock() { k_mutex_unlock(&m_); }
    bool try_lock() { return k_mutex_lock(&m_, K_NO_WAIT) == 0; }

    k_mutex* native_handle() { return &m_; }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
private:
    k_mutex m_;
};

class ConditionVariable {
public:
    ConditionVariable()  { k_condvar_init(&cv_); }
    ~ConditionVariable() = default;

    void notify_one() { k_condvar_signal(&cv_); }
    void notify_all() { k_condvar_broadcast(&cv_); }

    void wait(Mutex& mtx) {
        k_condvar_wait(&cv_, mtx.native_handle(), K_FOREVER);
    }

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

class Thread {
public:
    Thread() = default;

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

    ~Thread() {
        if (joinable()) {
            k_thread_abort(&thread_);
            delete ctx_;
            ctx_ = nullptr;
            started_ = false;
        }
    }

    bool joinable() const { return started_ && !joined_; }

    void join() {
        if (joinable()) {
            k_thread_join(&thread_, K_FOREVER);
            joined_ = true;
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
