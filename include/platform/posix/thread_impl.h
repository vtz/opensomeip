/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_THREAD_IMPL_H
#define SOMEIP_PLATFORM_POSIX_THREAD_IMPL_H

/**
 * @brief POSIX/Host threading backend.
 */

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <exception>
#ifdef __cpp_exceptions
#include <system_error>
#endif

#include "platform/host/host_condition_variable.h"

namespace someip {
namespace platform {

/** @implements REQ_PLATFORM_POSIX_001, REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_MUTEX_NONCOPY */
using Mutex = std::mutex;

/** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_THREAD_NONCOPY, REQ_PAL_THREAD_CREATE_E01, REQ_PAL_THREAD_DTOR_E01 */
class Thread {
public:
    Thread() = default;

    /** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_CREATE_E01 */
    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args) {
#ifdef __cpp_exceptions
        try {
            thread_ = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
        } catch (const std::system_error&) {
            // thread_ remains default-constructed (non-joinable)
        }
#else
        thread_ = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
#endif
    }

    /**
     * @implements REQ_PAL_THREAD_DTOR_E01
     * Callers must explicitly join() or detach() before destruction.
     * A joinable thread at destruction is a fatal programming error.
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

    // Copy and move are disabled to match RTOS non-transferable thread handles
    // (REQ_PAL_THREAD_NONCOPY) and prevent ownership transfer of the underlying
    // std::thread.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;

private:
    std::thread thread_;
};

namespace this_thread {
/** @implements REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO */
using std::this_thread::sleep_for;
} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_POSIX_THREAD_IMPL_H
