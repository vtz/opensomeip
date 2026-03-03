/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H
#define SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace someip {
namespace platform {

using Thread = std::thread;
using Mutex = std::mutex;

class ConditionVariable {
public:
    void notify_one() { cv_.notify_one(); }
    void notify_all() { cv_.notify_all(); }

    void wait(Mutex& m) {
        std::unique_lock<std::mutex> lk(m, std::adopt_lock);
        cv_.wait(lk);
        lk.release();
    }

    template <typename Pred>
    void wait(Mutex& m, Pred pred) {
        std::unique_lock<std::mutex> lk(m, std::adopt_lock);
        cv_.wait(lk, pred);
        lk.release();
    }

    ConditionVariable() = default;
    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

private:
    std::condition_variable cv_;
};

namespace this_thread {
using std::this_thread::sleep_for;
} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H
