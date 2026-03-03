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

/**
 * @brief Unit tests for the host ConditionVariable PAL wrapper.
 *
 * Covers the three bugs found during code review and fixed in
 * include/platform/host/host_condition_variable.h:
 *
 *  1. Correct lk.release()/guard.dismiss() order — after wait() returns
 *     normally, the caller must still own the mutex (unique_lock must NOT
 *     unlock it in its destructor).
 *  2. Exception safety — if the predicate throws, the ReleaseOnExit guard
 *     must still call lk.release() so the caller's mutex is not spuriously
 *     unlocked by unique_lock's destructor.
 *  3. API contract — wait(Mutex&) and wait(Mutex&, Pred) present the same
 *     interface as the Zephyr-side ConditionVariable so call-sites are
 *     cross-platform.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "platform/thread.h"

using someip::platform::ConditionVariable;
using someip::platform::Mutex;

namespace {

// Helper: notify cv after a short delay, used to unblock wait() calls.
std::thread make_notifier(ConditionVariable& cv, std::chrono::milliseconds delay = std::chrono::milliseconds(20)) {
    return std::thread([&cv, delay]() {
        std::this_thread::sleep_for(delay);
        cv.notify_one();
    });
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Mutex ownership preserved after wait() — regression for the inverted
//    lk.release()/guard.dismiss() order bug.
//    If the bug were present, unique_lock's destructor would unlock the mutex
//    before the caller's m.unlock() below, causing double-unlock (UB / crash).
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, CallerRetainsMutexOwnershipAfterWait) {
    Mutex m;
    ConditionVariable cv;

    auto notifier = make_notifier(cv);

    m.lock();
    cv.wait(m);
    // Mutex must still be locked by us here. A second thread must NOT be
    // able to acquire it at this point.
    std::atomic<bool> other_locked{false};
    std::thread probe([&]() { other_locked = m.try_lock(); });
    probe.join();
    EXPECT_FALSE(other_locked) << "mutex must still be held by the caller after wait() returns";

    m.unlock(); // must not double-unlock
    notifier.join();
}

// ---------------------------------------------------------------------------
// 2. Mutex ownership preserved after wait(pred) — same regression, predicate
//    overload.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, CallerRetainsMutexOwnershipAfterWaitWithPredicate) {
    Mutex m;
    ConditionVariable cv;
    bool ready = false;

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        m.lock();
        ready = true;
        m.unlock();
        cv.notify_one();
    });

    m.lock();
    cv.wait(m, [&]() { return ready; });

    // Mutex must still be locked by us here.
    std::atomic<bool> other_locked{false};
    std::thread probe([&]() { other_locked = m.try_lock(); });
    probe.join();
    EXPECT_FALSE(other_locked) << "mutex must still be held by the caller after wait(pred) returns";

    m.unlock();
    producer.join();
}

// ---------------------------------------------------------------------------
// 3. Exception safety — if the predicate throws, the RAII guard must call
//    lk.release() so the caller's mutex is not unlocked by unique_lock's dtor.
//    Regression for the missing/skipped lk.release() on exceptional paths.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, PredicateExceptionPreservesMutexOwnership) {
    Mutex m;
    ConditionVariable cv;

    // A notifier so cv_.wait() can return and the predicate is actually called.
    auto notifier = make_notifier(cv);

    m.lock();
    auto throwing_pred = []() -> bool { throw std::runtime_error("predicate threw"); };
    EXPECT_THROW(cv.wait(m, throwing_pred), std::runtime_error);

    // After the exception the mutex must still be owned by the caller.
    std::atomic<bool> other_locked{false};
    std::thread probe([&]() { other_locked = m.try_lock(); });
    probe.join();
    EXPECT_FALSE(other_locked) << "mutex must still be held by caller after predicate throws";

    m.unlock();
    notifier.join();
}

// ---------------------------------------------------------------------------
// 4. notify_one wakes exactly one waiter out of several.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, NotifyOneWakesExactlyOneWaiter) {
    Mutex m;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    const int kWaiters = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&]() {
            m.lock();
            cv.wait(m);
            ++woken;
            m.unlock();
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    cv.notify_one();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(woken.load(), 1) << "notify_one must wake exactly one waiter";

    // Wake the rest to let threads exit cleanly.
    cv.notify_all();
    for (auto& t : waiters) t.join();
    EXPECT_EQ(woken.load(), kWaiters);
}

// ---------------------------------------------------------------------------
// 5. notify_all wakes every waiter.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, NotifyAllWakesAllWaiters) {
    Mutex m;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    const int kWaiters = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&]() {
            m.lock();
            cv.wait(m);
            ++woken;
            m.unlock();
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    cv.notify_all();
    for (auto& t : waiters) t.join();

    EXPECT_EQ(woken.load(), kWaiters) << "notify_all must wake all waiters";
}

// ---------------------------------------------------------------------------
// 6. Cross-platform API contract — verify that call-sites written against
//    the Zephyr ConditionVariable API compile and run on host without changes.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, ZephyrAPIContractCompilable) {
    // This test documents and enforces the interface contract:
    //   cv.wait(mutex_ref)
    //   cv.wait(mutex_ref, predicate)
    //   cv.notify_one()
    //   cv.notify_all()
    // All must compile and run with someip::platform::Mutex (== std::mutex on host).

    Mutex m;
    ConditionVariable cv;
    bool flag = false;

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        m.lock();
        flag = true;
        m.unlock();
        cv.notify_one(); // Zephyr API call-site
    });

    m.lock();
    cv.wait(m, [&]() { return flag; }); // Zephyr API call-site
    m.unlock();

    t.join();
    EXPECT_TRUE(flag);
}
