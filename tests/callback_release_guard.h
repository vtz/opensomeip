/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_TEST_CALLBACK_RELEASE_GUARD_H
#define SOMEIP_TEST_CALLBACK_RELEASE_GUARD_H

#include <future>

namespace someip::test {

// Declare after all async futures so failure-path unwinding releases callbacks before joining.
class CallbackReleaseGuard {
   public:
    explicit CallbackReleaseGuard(std::promise<void>& release) : release_(release)
    {
    }
    ~CallbackReleaseGuard()
    {
        release();
    }
    CallbackReleaseGuard(const CallbackReleaseGuard&) = delete;
    CallbackReleaseGuard& operator=(const CallbackReleaseGuard&) = delete;
    CallbackReleaseGuard(CallbackReleaseGuard&&) = delete;
    CallbackReleaseGuard& operator=(CallbackReleaseGuard&&) = delete;

    void release()
    {
        if (!released_) {
            release_.set_value();
            released_ = true;
        }
    }

   private:
    std::promise<void>& release_;
    bool released_{false};
};

}  // namespace someip::test

#endif  // SOMEIP_TEST_CALLBACK_RELEASE_GUARD_H
