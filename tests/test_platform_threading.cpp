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
#include <vector>

#include "platform/thread.h"

using someip::platform::ConditionVariable;
using someip::platform::Mutex;

// ---------------------------------------------------------------------------
// 1. Mutex ownership preserved after wait(pred) — regression for the inverted
//    lk.release()/guard.dismiss() order bug.
//    Uses a state flag so spurious wakeups cannot cause a false pass: the
//    wait only exits when the notifier has actually set notified=true, and we
//    assert that flag is observed after returning.
//    If the bug were present, unique_lock's destructor would unlock the mutex
//    before the caller's m.unlock() below, causing double-unlock (UB / crash).
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, CallerRetainsMutexOwnershipAfterWait) {
    Mutex m;
    ConditionVariable cv;
    bool notified = false;

    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        m.lock();
        notified = true;
        m.unlock();
        cv.notify_one();
    });

    m.lock();
    cv.wait(m, [&]{ return notified; });
    EXPECT_TRUE(notified) << "wait must not return until the flag is set";

    // Mutex must still be locked by us here. A second thread must NOT be
    // able to acquire it at this point.
    std::atomic<bool> other_locked{false};
    std::thread probe([&]() {
        if (m.try_lock()) {
            other_locked = true;
            m.unlock(); // probe always cleans up its own acquisition
        }
    });
    probe.join();
    EXPECT_FALSE(other_locked) << "mutex must still be held by the caller after wait() returns";

    if (!other_locked) m.unlock(); // only unlock if the main thread still owns it
    notifier.join();
}

// ---------------------------------------------------------------------------
// 2. Mutex ownership preserved after wait(pred) — predicate overload,
//    notifier sets the flag under the mutex before signalling.
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
    std::thread probe([&]() {
        if (m.try_lock()) {
            other_locked = true;
            m.unlock(); // probe always cleans up its own acquisition
        }
    });
    probe.join();
    EXPECT_FALSE(other_locked) << "mutex must still be held by the caller after wait(pred) returns";

    if (!other_locked) m.unlock(); // only unlock if the main thread still owns it
    producer.join();
}

// ---------------------------------------------------------------------------
// 3. Exception safety — if the predicate throws *after being woken by a real
//    notification*, the ReleaseOnExit guard must call lk.release() so the
//    caller's mutex is not unlocked by unique_lock's dtor.
//
//    The predicate returns false until notified=true, then throws.  This
//    ensures the exception propagates through the internal cv_.wait loop
//    (not just on the initial predicate check before any suspension), which
//    is the path that exercises the RAII guard.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, PredicateExceptionPreservesMutexOwnership) {
    Mutex m;
    ConditionVariable cv;
    bool notified = false;

    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        m.lock();
        notified = true;
        m.unlock();
        cv.notify_one();
    });

    // Predicate: keeps the waiter sleeping until notified, then throws so
    // the exception path through cv_.wait(lk, pred) is exercised.
    auto throwing_pred = [&]() -> bool {
        if (notified) throw std::runtime_error("predicate threw");
        return false;
    };

    m.lock();
    EXPECT_THROW(cv.wait(m, throwing_pred), std::runtime_error);

    // After the exception the mutex must still be owned by the caller.
    std::atomic<bool> other_locked{false};
    std::thread probe([&]() {
        if (m.try_lock()) {
            other_locked = true;
            m.unlock(); // probe always cleans up its own acquisition
        }
    });
    probe.join();
    EXPECT_FALSE(other_locked) << "mutex must still be held by caller after predicate throws";

    if (!other_locked) m.unlock(); // only unlock if the main thread still owns it
    notifier.join();
}

// ---------------------------------------------------------------------------
// 4. notify_one wakes exactly one waiter out of several.
//
//    Uses a wake_permits counter (guarded by m) as the predicate condition.
//    Each waiter consumes one permit on exit, so a spurious wakeup that
//    checks the predicate while permits == 0 simply goes back to sleep —
//    the "exactly one" assertion cannot be broken by spurious wakeups.
// ---------------------------------------------------------------------------
TEST(ConditionVariableTest, NotifyOneWakesExactlyOneWaiter) {
    Mutex m;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    int wake_permits = 0; // guarded by m; each waiter consumes one permit on exit
    const int kWaiters = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&]() {
            m.lock();
            cv.wait(m, [&]{ return wake_permits > 0; });
            --wake_permits; // consume permit before releasing the mutex
            ++woken;
            m.unlock();
        });
    }

    // Let all waiters settle into the predicate-wait state.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Grant exactly one permit and wake one waiter.
    { m.lock(); wake_permits = 1; m.unlock(); }
    cv.notify_one();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(woken.load(), 1) << "notify_one must wake exactly one waiter";

    // Grant enough permits for all remaining waiters and unblock them.
    { m.lock(); wake_permits = kWaiters; m.unlock(); }
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
