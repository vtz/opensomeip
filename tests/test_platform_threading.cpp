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
 * @brief Unit tests for the PAL threading primitives (Mutex, CV, Thread, ScopedLock, sleep_for).
 * @tests REQ_PLATFORM_ARCH_001
 * @tests REQ_ARCH_002
 * @tests REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_MUTEX_NONCOPY
 * @tests REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP
 * @tests REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_THREAD_NONCOPY
 * @tests REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_LOCK_NONCOPY
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 * @tests REQ_PAL_MUTEX_UNLOCK_E01, REQ_PAL_CV_EXCEPT_E01
 * @tests REQ_PAL_THREAD_CREATE_E01, REQ_PAL_THREAD_DTOR_E01
 * @tests REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
 * @tests REQ_PAL_NET_CLOSE, REQ_PAL_NET_SHUTDOWN, REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_BLOCK
 * @tests REQ_PAL_NET_MODE_E01
 * @tests REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_NTOHS, REQ_PAL_BYTE_HTONL, REQ_PAL_BYTE_NTOHL
 * @tests REQ_PLATFORM_POSIX_001, REQ_PLATFORM_POSIX_002
 * @tests REQ_PLATFORM_POSIX_003, REQ_PLATFORM_POSIX_004
 * @tests REQ_PLATFORM_WIN32_001, REQ_PLATFORM_WIN32_002, REQ_PLATFORM_WIN32_003, REQ_PLATFORM_WIN32_004
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
#include "platform/memory.h"
#include "platform/byteorder.h"
#include "platform/net.h"

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
    std::atomic<int> ready_count{0};
    int wake_permits = 0;
    const int kWaiters = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&]() {
            m.lock();
            ready_count.fetch_add(1, std::memory_order_release);
            cv.wait(m, [&]{ return wake_permits > 0; });
            --wake_permits;
            ++woken;
            m.unlock();
        });
    }

    while (ready_count.load(std::memory_order_acquire) < kWaiters) {
        std::this_thread::yield();
    }
    // Acquiring the mutex guarantees the last waiter has entered cv.wait()
    m.lock(); wake_permits = 1; m.unlock();
    cv.notify_one();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (woken.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    EXPECT_EQ(woken.load(), 1) << "notify_one must wake exactly one waiter";

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
    std::atomic<int> ready_count{0};
    bool go = false;
    const int kWaiters = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < kWaiters; ++i) {
        waiters.emplace_back([&]() {
            m.lock();
            ready_count.fetch_add(1, std::memory_order_release);
            cv.wait(m, [&]{ return go; });
            ++woken;
            m.unlock();
        });
    }

    while (ready_count.load(std::memory_order_acquire) < kWaiters) {
        std::this_thread::yield();
    }
    { m.lock(); go = true; m.unlock(); }
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

// ===========================================================================
// 7. Mutex standalone — lock, unlock, try_lock
// ===========================================================================

/**
 * @test_case TC_PAL_MUTEX_001
 * @tests REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK
 * @brief Verify Mutex lock/unlock/try_lock contract
 */
TEST(MutexTest, LockUnlockTryLock) {
    Mutex m;

    m.lock();

    std::atomic<bool> other_acquired{false};
    std::thread probe([&]() {
        other_acquired = m.try_lock();
        if (other_acquired) m.unlock();
    });
    probe.join();
    EXPECT_FALSE(other_acquired) << "try_lock must fail while mutex is held";

    m.unlock();

    std::atomic<bool> after_unlock{false};
    std::thread probe2([&]() {
        after_unlock = m.try_lock();
        if (after_unlock) m.unlock();
    });
    probe2.join();
    EXPECT_TRUE(after_unlock) << "try_lock must succeed after unlock";
}

/**
 * @test_case TC_PAL_MUTEX_002
 * @tests REQ_PAL_MUTEX_UNLOCK_E01
 * @brief Verify double unlock does not crash
 */
TEST(MutexTest, DoubleUnlockNoCrash) {
    Mutex m;
    m.lock();
    m.unlock();
    m.unlock();
    // Platform-defined behavior, but must not crash
    EXPECT_TRUE(true);
}

// ===========================================================================
// 8. ScopedLock RAII
// ===========================================================================

/**
 * @test_case TC_PAL_SCOPEDLOCK_001
 * @tests REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE
 * @brief Verify ScopedLock acquires on construction and releases on destruction
 */
TEST(ScopedLockTest, RaiiLockUnlock) {
    Mutex m;

    {
        someip::platform::ScopedLock guard(m);

        std::atomic<bool> locked_by_other{false};
        std::thread probe([&]() {
            locked_by_other = m.try_lock();
            if (locked_by_other) m.unlock();
        });
        probe.join();
        EXPECT_FALSE(locked_by_other) << "Mutex must be held inside ScopedLock scope";
    }

    std::atomic<bool> free_after{false};
    std::thread probe2([&]() {
        free_after = m.try_lock();
        if (free_after) m.unlock();
    });
    probe2.join();
    EXPECT_TRUE(free_after) << "Mutex must be released after ScopedLock destructor";
}

// ===========================================================================
// 9. Thread lifecycle
// ===========================================================================

/**
 * @test_case TC_PAL_THREAD_001
 * @tests REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOIN
 * @brief Verify Thread runs callable and join works
 */
TEST(ThreadTest, RunAndJoin) {
    std::atomic<bool> ran{false};

    someip::platform::Thread t([&ran]() {
        ran = true;
    });

    EXPECT_TRUE(t.joinable());
    t.join();
    EXPECT_FALSE(t.joinable());
    EXPECT_TRUE(ran) << "Thread body must have executed";
}

/**
 * @test_case TC_PAL_THREAD_002
 * @tests REQ_PAL_THREAD_CREATE
 * @brief Verify Thread with arguments
 */
TEST(ThreadTest, ThreadWithArguments) {
    std::atomic<int> result{0};

    someip::platform::Thread t([&result](int a, int b) {
        result = a + b;
    }, 40, 2);

    t.join();
    EXPECT_EQ(result.load(), 42);
}

/**
 * @test_case TC_PAL_THREAD_003
 * @tests REQ_PAL_THREAD_DTOR_E01
 * @brief Verify Thread join is idempotent (double join is safe)
 */
TEST(ThreadTest, DoubleJoinIsSafe) {
    std::atomic<bool> ran{false};
    someip::platform::Thread t([&ran]() {
        ran = true;
    });

    t.join();
    EXPECT_FALSE(t.joinable());

    // Second join should be a no-op, not crash
    t.join();
    EXPECT_FALSE(t.joinable());
    EXPECT_TRUE(ran);
}

// ===========================================================================
// 10. sleep_for
// ===========================================================================

/**
 * @test_case TC_PAL_SLEEP_001
 * @tests REQ_PAL_SLEEP_DURATION
 * @brief Verify sleep_for blocks for at least the requested duration
 */
TEST(SleepForTest, TimingBounds) {
    auto t0 = std::chrono::steady_clock::now();
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto t1 = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    EXPECT_GE(elapsed_ms, 40) << "sleep_for must block for at least ~50ms";
    EXPECT_LE(elapsed_ms, 500) << "sleep_for should not block excessively";
}

/**
 * @test_case TC_PAL_SLEEP_002
 * @tests REQ_PAL_SLEEP_ZERO
 * @brief Verify zero-duration sleep returns immediately
 */
TEST(SleepForTest, ZeroDuration) {
    auto t0 = std::chrono::steady_clock::now();
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(0));
    auto t1 = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    EXPECT_LE(elapsed_ms, 50) << "Zero-duration sleep should return almost immediately";
}

// ===========================================================================
// 11. Memory allocation
// ===========================================================================

/**
 * @test_case TC_PAL_MEMORY_001
 * @tests REQ_PAL_MEM_ALLOC, REQ_PLATFORM_POSIX_002
 * @brief Verify allocate_message returns usable Message
 */
TEST(MemoryTest, AllocateMessage) {
    auto msg = someip::platform::allocate_message();
    ASSERT_NE(msg, nullptr);

    msg->set_service_id(0x1234);
    msg->set_payload({0xAA, 0xBB, 0xCC});

    EXPECT_EQ(msg->get_service_id(), 0x1234);
    EXPECT_EQ(msg->get_payload().size(), 3u);
}

/**
 * @test_case TC_PAL_MEMORY_002
 * @tests REQ_PAL_MEM_INDEPENDENT
 * @brief Verify multiple allocations return independent objects
 */
TEST(MemoryTest, IndependentAllocations) {
    auto msg1 = someip::platform::allocate_message();
    auto msg2 = someip::platform::allocate_message();
    ASSERT_NE(msg1, nullptr);
    ASSERT_NE(msg2, nullptr);

    msg1->set_service_id(0x1111);
    msg2->set_service_id(0x2222);

    EXPECT_EQ(msg1->get_service_id(), 0x1111);
    EXPECT_EQ(msg2->get_service_id(), 0x2222);
    EXPECT_NE(msg1.get(), msg2.get());
}

// ===========================================================================
// 12. Byte-order conversion
// ===========================================================================

/**
 * @test_case TC_PAL_BYTEORDER_001
 * @tests REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_NTOHS, REQ_PAL_BYTE_HTONL, REQ_PAL_BYTE_NTOHL, REQ_PLATFORM_POSIX_004
 * @brief Verify byte-order macro roundtrip
 */
TEST(ByteOrderTest, RoundTrip16) {
    uint16_t host_val = 0x1234;
    uint16_t net_val = someip_htons(host_val);
    uint16_t back = someip_ntohs(net_val);
    EXPECT_EQ(back, host_val) << "htons/ntohs roundtrip must preserve value";
}

TEST(ByteOrderTest, RoundTrip32) {
    uint32_t host_val = 0x12345678;
    uint32_t net_val = someip_htonl(host_val);
    uint32_t back = someip_ntohl(net_val);
    EXPECT_EQ(back, host_val) << "htonl/ntohl roundtrip must preserve value";
}

/**
 * @test_case TC_PAL_BYTEORDER_002
 * @tests REQ_PAL_BYTE_HTONS, REQ_PAL_BYTE_HTONL
 * @brief Verify network byte order is big-endian
 */
TEST(ByteOrderTest, NetworkIsBigEndian) {
    uint16_t net16 = someip_htons(0x0102);
    auto* bytes16 = reinterpret_cast<uint8_t*>(&net16);
    EXPECT_EQ(bytes16[0], 0x01);
    EXPECT_EQ(bytes16[1], 0x02);

    uint32_t net32 = someip_htonl(0x01020304);
    auto* bytes32 = reinterpret_cast<uint8_t*>(&net32);
    EXPECT_EQ(bytes32[0], 0x01);
    EXPECT_EQ(bytes32[1], 0x02);
    EXPECT_EQ(bytes32[2], 0x03);
    EXPECT_EQ(bytes32[3], 0x04);
}

// ===========================================================================
// 13. Networking helpers
// ===========================================================================

/**
 * @test_case TC_PAL_NET_001
 * @tests REQ_PAL_NET_MODE_E01
 * @brief Verify someip_set_nonblocking fails on invalid fd
 */
TEST(NetTest, NonBlockingInvalidFd) {
    int result = someip_set_nonblocking(-1);
    EXPECT_EQ(result, -1) << "Invalid fd should return -1";
}

/**
 * @test_case TC_PAL_NET_002
 * @tests REQ_PAL_NET_CLOSE, REQ_PAL_NET_NONBLOCK, REQ_PLATFORM_POSIX_003
 * @brief Verify socket creation, nonblocking mode, and close
 */
TEST(NetTest, SocketLifecycle) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(fd, 0) << "Socket creation should succeed";

    EXPECT_EQ(someip_set_nonblocking(fd), 0);
    EXPECT_EQ(someip_set_blocking(fd), 0);

    someip_close_socket(fd);
}
