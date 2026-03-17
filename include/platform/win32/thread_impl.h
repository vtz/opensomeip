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
#include <functional>
#include <system_error>

#include "platform/host/host_condition_variable.h"

namespace someip {
namespace platform {

using Mutex = std::mutex;

/** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_THREAD_NONCOPY, REQ_PAL_THREAD_CREATE_E01, REQ_PAL_THREAD_DTOR_E01 */
class Thread {
public:
    Thread() = default;

    /** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_CREATE_E01 */
    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args) {
        try {
            thread_ = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
            started_ = true;
        } catch (const std::system_error&) {
            // thread_ remains default-constructed (non-joinable)
        }
    }

    /**
     * @implements REQ_PAL_THREAD_DTOR_E01
     * Callers must explicitly join() or detach() before destruction.
     */
    ~Thread() {
        if (thread_.joinable()) {
            std::terminate();
        }
    }

    /** @implements REQ_PAL_THREAD_JOINABLE */
    bool joinable() const { return thread_.joinable(); }

    /** @implements REQ_PAL_THREAD_JOIN */
    void join() {
        if (thread_.joinable() &&
            thread_.get_id() != std::this_thread::get_id()) {
            thread_.join();
        }
    }

    bool started() const { return started_; }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;

private:
    std::thread thread_;
    bool started_{false};
};

namespace this_thread {
using std::this_thread::sleep_for;
} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H
