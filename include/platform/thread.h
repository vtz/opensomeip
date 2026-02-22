/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_THREAD_H
#define SOMEIP_PLATFORM_THREAD_H

/**
 * @brief Portable threading primitives.
 *
 * On host / native_sim builds, this simply aliases std::thread,
 * std::mutex, etc.  On embedded Zephyr targets (Cortex-M), this
 * provides thin wrappers around k_thread / k_mutex / k_condvar.
 */

#if defined(__ZEPHYR__) && !defined(CONFIG_NATIVE_APPLICATION)
/* ------------------------------------------------------------------ */
/* Zephyr embedded: k_thread / k_mutex / k_condvar wrappers          */
/* ------------------------------------------------------------------ */
#include <zephyr/kernel.h>
#include <functional>
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

    void lock()   { k_mutex_lock(&m_, K_FOREVER); }
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
        entry_ = [f = std::forward<Fn>(fn),
                   a = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            std::apply(std::move(f), std::move(a));
        };
        k_thread_create(&thread_, stack_, sizeof(stack_),
                        trampoline, this, nullptr, nullptr,
                        K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    }

    ~Thread() = default;

    bool joinable() const { return joined_ == false && entry_ != nullptr; }

    void join() {
        if (joinable()) {
            k_thread_join(&thread_, K_FOREVER);
            joined_ = true;
        }
    }

    Thread(Thread&& o) noexcept
        : entry_(std::move(o.entry_)), joined_(o.joined_) {
        std::memcpy(&thread_, &o.thread_, sizeof(thread_));
        std::memcpy(stack_, o.stack_, sizeof(stack_));
        o.entry_ = nullptr;
        o.joined_ = true;
    }

    Thread& operator=(Thread&& o) noexcept {
        if (this != &o) {
            if (joinable()) join();
            entry_ = std::move(o.entry_);
            joined_ = o.joined_;
            std::memcpy(&thread_, &o.thread_, sizeof(thread_));
            std::memcpy(stack_, o.stack_, sizeof(stack_));
            o.entry_ = nullptr;
            o.joined_ = true;
        }
        return *this;
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

private:
    static void trampoline(void* p1, void*, void*) {
        auto* self = static_cast<Thread*>(p1);
        if (self->entry_) self->entry_();
    }

    k_thread thread_{};
    K_THREAD_STACK_MEMBER(stack_, CONFIG_SOMEIP_THREAD_STACK_SIZE);
    std::function<void()> entry_;
    bool joined_{false};
};

} // namespace platform
} // namespace someip

/* Scoped-lock helper compatible with platform::Mutex */
namespace someip {
namespace platform {
class ScopedLock {
public:
    explicit ScopedLock(Mutex& m) : m_(m) { m_.lock(); }
    ~ScopedLock() { m_.unlock(); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
private:
    Mutex& m_;
};
} // namespace platform
} // namespace someip

#else
/* ------------------------------------------------------------------ */
/* Host / native_sim: standard C++ threading                          */
/* ------------------------------------------------------------------ */
#include <thread>
#include <mutex>
#include <condition_variable>

namespace someip {
namespace platform {
using Thread = std::thread;
using Mutex = std::mutex;
using ConditionVariable = std::condition_variable;
using ScopedLock = std::scoped_lock<std::mutex>;
} // namespace platform
} // namespace someip

#endif

/* Convenience aliases so call sites can write platform::this_thread::sleep_for */
namespace someip {
namespace platform {
namespace this_thread {
#if defined(__ZEPHYR__) && !defined(CONFIG_NATIVE_APPLICATION)
    template <typename Rep, typename Period>
    void sleep_for(const std::chrono::duration<Rep, Period>& d) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
        k_msleep(static_cast<int32_t>(ms));
    }
#else
    using std::this_thread::sleep_for;
#endif
} // namespace this_thread
} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_THREAD_H
