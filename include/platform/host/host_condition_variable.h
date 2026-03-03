/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_HOST_CONDITION_VARIABLE_H
#define SOMEIP_PLATFORM_HOST_CONDITION_VARIABLE_H

#include <mutex>
#include <condition_variable>

namespace someip {
namespace platform {

// ConditionVariable wrapper that matches the Zephyr RTOS API:
//   wait(Mutex&) and wait(Mutex&, Pred)
//
// On host, Mutex == std::mutex. The wrapper uses std::unique_lock internally
// and an RAII ReleaseOnExit guard to ensure lk.release() is called on every
// exit path (normal and exceptional), so the caller retains mutex ownership.
class ConditionVariable {
public:
    void notify_one() { cv_.notify_one(); }
    void notify_all() { cv_.notify_all(); }

    void wait(std::mutex& m) {
        std::unique_lock<std::mutex> lk(m, std::adopt_lock);
        ReleaseOnExit guard(lk);
        cv_.wait(lk);
        lk.release();    // transfer ownership back to caller without unlocking
        guard.dismiss(); // guard's destructor must not call release() again
    }

    template <typename Pred>
    void wait(std::mutex& m, Pred pred) {
        std::unique_lock<std::mutex> lk(m, std::adopt_lock);
        ReleaseOnExit guard(lk);
        cv_.wait(lk, pred);
        lk.release();    // transfer ownership back to caller without unlocking
        guard.dismiss(); // guard's destructor must not call release() again
    }

    ConditionVariable() = default;
    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

private:
    // On the normal path, wait() calls lk.release() then guard.dismiss():
    //   lk.release() hands mutex ownership back to the caller without unlocking;
    //   dismiss() prevents the destructor from calling release() a second time.
    // On the exception path (cv_.wait or predicate throws), dismiss() is never
    //   reached so the destructor calls lk.release(), which also hands ownership
    //   back to the caller — preventing unique_lock's destructor from unlocking
    //   the caller-owned mutex.
    struct ReleaseOnExit {
        explicit ReleaseOnExit(std::unique_lock<std::mutex>& lk) : lk_(lk) {}
        ~ReleaseOnExit() { if (!dismissed_) lk_.release(); }
        void dismiss() { dismissed_ = true; }
    private:
        std::unique_lock<std::mutex>& lk_;
        bool dismissed_{false};
    };

    std::condition_variable cv_;
};

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_HOST_CONDITION_VARIABLE_H
