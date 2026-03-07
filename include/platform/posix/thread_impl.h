/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_THREAD_IMPL_H
#define SOMEIP_PLATFORM_POSIX_THREAD_IMPL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <tuple>

#include "platform/host/host_condition_variable.h"

namespace someip {
namespace platform {

using Mutex = std::mutex;

class Thread {
public:
    Thread() = default;

    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args)
        : thread_(std::forward<Fn>(fn), std::forward<Args>(args)...) {}

    ~Thread() {
        if (thread_.joinable()) thread_.detach();
    }

    bool joinable() const { return thread_.joinable(); }

    void join() {
        if (thread_.joinable()) thread_.join();
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;

private:
    std::thread thread_;
};

namespace this_thread {
using std::this_thread::sleep_for;
} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_POSIX_THREAD_IMPL_H
