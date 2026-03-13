/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_FAKE_THREAD_IMPL_H
#define SOMEIP_FAKE_THREAD_IMPL_H

/**
 * @brief Recording-fake PAL types for protocol-layer unit tests.
 *
 * Compile protocol code with -I tests/fakes/ instead of a real backend.
 * The fake types record calls (lock_count, unlock_count, etc.) so tests
 * can assert that protocol layers use synchronization correctly, without
 * any real threading or blocking.
 *
 * Example usage in a test:
 *   // After running some protocol operation:
 *   EXPECT_GE(my_mutex.lock_count, 1);
 *   EXPECT_EQ(my_mutex.lock_count, my_mutex.unlock_count);
 */

#include <functional>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace someip {
namespace platform {

class Mutex {
public:
    void lock()   { ++lock_count;    locked = true;  }
    void unlock() { ++unlock_count;  locked = false; }
    bool try_lock() {
        ++try_lock_count;
        if (!locked) { locked = true; ++lock_count; return true; }
        return false;
    }

    Mutex() = default;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    int  lock_count{0};
    int  unlock_count{0};
    int  try_lock_count{0};
    bool locked{false};
};

class ConditionVariable {
public:
    void notify_one() { ++notify_one_count; }
    void notify_all() { ++notify_all_count; }

    void wait(Mutex& m) {
        ++wait_count;
        m.unlock();
        // In the fake, immediately re-acquire — no real blocking.
        m.lock();
    }

    template <typename Pred>
    void wait(Mutex& m, Pred pred) {
        ++wait_pred_count;
        while (!pred()) {
            m.unlock();
            std::this_thread::yield();
            m.lock();
        }
    }

    ConditionVariable() = default;
    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

    int notify_one_count{0};
    int notify_all_count{0};
    int wait_count{0};
    int wait_pred_count{0};
};

class Thread {
public:
    Thread() = default;

    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args) {
        started_ = true;
        auto callable = std::bind(std::forward<Fn>(fn),
                                  std::forward<Args>(args)...);
        callable();
        finished_ = true;
    }

    ~Thread() {
        if (joinable()) {
            ++dtor_without_join_count;
        }
    }

    bool joinable() const { return started_ && !joined_; }

    bool started() const { return started_; }

    void join() {
        if (joinable()) {
            joined_ = true;
            ++join_count;
        }
    }

    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    int join_count{0};
    int dtor_without_join_count{0};

private:
    bool started_{false};
    bool joined_{false};
    bool finished_{false};
};

namespace this_thread {

template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& /*d*/) {
    // No-op in the fake — protocol tests should not depend on real delays.
}

} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_FAKE_THREAD_IMPL_H
