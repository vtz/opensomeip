/********************************************************************************
 * PAL Conformance Test Suite
 *
 * Shared test cases included by test_pal_{freertos,threadx,zephyr}_mock.cpp.
 * Each wrapper compiles this file with a different backend selected via
 * include paths:
 *   -I tests/mocks/<backend>          (mock RTOS headers)
 *   -I include                         (PAL public headers)
 *   -I include/platform/<backend>      (backend thread_impl.h / memory_impl.h)
 *
 * Tests cover:
 *   Mutex:      lock, unlock, try_lock, non-copyable, double-unlock
 *   CV:         wait, wait+pred, notify_one, notify_all, ownership
 *   Thread:     create, joinable, join (idempotent)
 *   ScopedLock: acquire-on-construct, release-on-destruct, non-copyable
 *   sleep_for:  minimum duration, zero duration
 *   Memory:     allocate, independent objects
 ********************************************************************************/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>
#include <vector>

#include "platform/thread.h"
#include "platform/memory.h"

using someip::platform::Mutex;
using someip::platform::ConditionVariable;
using someip::platform::ScopedLock;

/* ======================================================================== */
/*  Mutex Conformance                                                       */
/* ======================================================================== */

/**
 * @test_case TC_MOCK_MUTEX_LOCK
 * @tests REQ_PAL_MUTEX_LOCK
 * @brief lock() acquires exclusive ownership — try_lock from another thread fails.
 */
TEST(MutexConformance, LockBlocksOtherThread) {
    Mutex m;
    m.lock();

    std::atomic<bool> acquired{false};
    std::thread probe([&] {
        acquired = m.try_lock();
        if (acquired) m.unlock();
    });
    probe.join();
    EXPECT_FALSE(acquired);
    m.unlock();
}

/**
 * @test_case TC_MOCK_MUTEX_UNLOCK
 * @tests REQ_PAL_MUTEX_UNLOCK
 * @brief unlock() releases ownership — try_lock from another thread succeeds.
 */
TEST(MutexConformance, UnlockReleasesOwnership) {
    Mutex m;
    m.lock();
    m.unlock();

    std::atomic<bool> acquired{false};
    std::thread probe([&] {
        acquired = m.try_lock();
        if (acquired) m.unlock();
    });
    probe.join();
    EXPECT_TRUE(acquired);
}

/**
 * @test_case TC_MOCK_MUTEX_TRYLOCK
 * @tests REQ_PAL_MUTEX_TRYLOCK
 * @brief try_lock() returns true when unheld, false when held by another thread.
 */
TEST(MutexConformance, TryLockNonBlocking) {
    Mutex m;
    EXPECT_TRUE(m.try_lock());

    std::atomic<bool> second{false};
    std::thread probe([&] {
        second = m.try_lock();
        if (second) m.unlock();
    });
    probe.join();
    EXPECT_FALSE(second);
    m.unlock();
}

/**
 * @test_case TC_MOCK_MUTEX_NONCOPY
 * @tests REQ_PAL_MUTEX_NONCOPY
 * @brief Mutex is non-copyable.
 */
TEST(MutexConformance, NonCopyable) {
    EXPECT_FALSE(std::is_copy_constructible<Mutex>::value);
    EXPECT_FALSE(std::is_copy_assignable<Mutex>::value);
}

/**
 * @test_case TC_MOCK_MUTEX_DOUBLE_UNLOCK
 * @tests REQ_PAL_MUTEX_UNLOCK_E01
 * @brief Double unlock does not crash or corrupt state.
 */
TEST(MutexConformance, DoubleUnlockNoCrash) {
    Mutex m;
    m.lock();
    m.unlock();
    m.unlock();
    SUCCEED();
}

/* ======================================================================== */
/*  ConditionVariable Conformance                                           */
/* ======================================================================== */

/**
 * @test_case TC_MOCK_CV_WAIT
 * @tests REQ_PAL_CV_WAIT, REQ_PAL_CV_OWNERSHIP
 * @brief wait() suspends until notify; mutex is held on return.
 */
TEST(CVConformance, WaitAndOwnership) {
    Mutex m;
    ConditionVariable cv;
    bool ready = false;

    std::thread notifier([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        m.lock();
        ready = true;
        m.unlock();
        cv.notify_one();
    });

    m.lock();
    cv.wait(m, [&] { return ready; });
    EXPECT_TRUE(ready);

    std::atomic<bool> probe_got{false};
    std::thread probe([&] {
        probe_got = m.try_lock();
        if (probe_got) m.unlock();
    });
    probe.join();
    EXPECT_FALSE(probe_got) << "Mutex must still be held after wait() returns";

    m.unlock();
    notifier.join();
}

/**
 * @test_case TC_MOCK_CV_WAIT_PRED
 * @tests REQ_PAL_CV_WAIT_PRED
 * @brief Predicate-based wait only returns when predicate is true.
 */
TEST(CVConformance, WaitWithPredicate) {
    Mutex m;
    ConditionVariable cv;
    int counter = 0;

    std::thread producer([&] {
        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            m.lock();
            ++counter;
            m.unlock();
            cv.notify_one();
        }
    });

    m.lock();
    cv.wait(m, [&] { return counter >= 3; });
    EXPECT_GE(counter, 3);
    m.unlock();
    producer.join();
}

/**
 * @test_case TC_MOCK_CV_NOTIFY_ONE
 * @tests REQ_PAL_CV_NOTIFY_ONE
 * @brief notify_one() wakes exactly one waiter.
 */
TEST(CVConformance, NotifyOneWakesOne) {
    Mutex m;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    int permits = 0;
    const int N = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < N; ++i) {
        waiters.emplace_back([&] {
            m.lock();
            cv.wait(m, [&] { return permits > 0; });
            --permits;
            ++woken;
            m.unlock();
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    { m.lock(); permits = 1; m.unlock(); }
    cv.notify_one();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(woken.load(), 1);

    { m.lock(); permits = N; m.unlock(); }
    cv.notify_all();
    for (auto& t : waiters) t.join();
    EXPECT_EQ(woken.load(), N);
}

/**
 * @test_case TC_MOCK_CV_NOTIFY_ALL
 * @tests REQ_PAL_CV_NOTIFY_ALL
 * @brief notify_all() wakes all waiters.
 */
TEST(CVConformance, NotifyAllWakesAll) {
    Mutex m;
    ConditionVariable cv;
    std::atomic<int> woken{0};
    bool go = false;
    const int N = 4;

    std::vector<std::thread> waiters;
    for (int i = 0; i < N; ++i) {
        waiters.emplace_back([&] {
            m.lock();
            cv.wait(m, [&] { return go; });
            ++woken;
            m.unlock();
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    m.lock(); go = true; m.unlock();
    cv.notify_all();
    for (auto& t : waiters) t.join();
    EXPECT_EQ(woken.load(), N);
}

/**
 * @test_case TC_MOCK_CV_NOTIFY_BEFORE_WAIT
 * @tests REQ_PAL_CV_NOTIFY_ONE
 * @brief Regression: notify_one() before any thread waits must not be lost.
 *
 * Reproduces the deadlock fixed in the ThreadX mock where
 * tx_event_flags_get used a pure generation-counter predicate.  If the
 * event was set before the waiter entered wait(), the generation had
 * already advanced and the waiter would block forever.
 */
TEST(CVConformance, NotifyBeforeWaitDoesNotDeadlock) {
    Mutex m;
    ConditionVariable cv;
    bool ready = false;

    m.lock();
    ready = true;
    m.unlock();
    cv.notify_one();

    std::thread waiter([&] {
        m.lock();
        cv.wait(m, [&] { return ready; });
        EXPECT_TRUE(ready);
        m.unlock();
    });
    waiter.join();
}

/**
 * @test_case TC_MOCK_CV_REPEATED_NOTIFY_WAIT
 * @tests REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_WAIT_PRED
 * @brief Regression: rapid signal/wait cycles must not lose wakeups.
 */
TEST(CVConformance, RepeatedNotifyWaitCycles) {
    Mutex m;
    ConditionVariable cv;
    int value = 0;
    bool done = false;

    std::thread producer([&] {
        for (int i = 1; i <= 5; ++i) {
            m.lock();
            value = i;
            m.unlock();
            cv.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        m.lock();
        done = true;
        m.unlock();
        cv.notify_one();
    });

    m.lock();
    cv.wait(m, [&] { return done; });
    EXPECT_TRUE(done);
    EXPECT_EQ(value, 5);
    m.unlock();

    producer.join();
}

/* ======================================================================== */
/*  Thread Conformance                                                      */
/* ======================================================================== */

/**
 * @test_case TC_MOCK_THREAD_JOIN_FAST
 * @tests REQ_PAL_THREAD_JOIN
 * @brief Regression: join() must succeed even when the thread body
 *        completes before join() is called (event already signalled).
 *
 * This is the exact pattern that caused a CI deadlock on the ThreadX
 * mock: the trampoline set the join event flag before the main thread
 * entered tx_event_flags_get, causing a missed wakeup.
 */
TEST(ThreadConformance, JoinAfterFastCompletion) {
    for (int i = 0; i < 20; ++i) {
        std::atomic<bool> ran{false};
        someip::platform::Thread t([&ran]() { ran = true; });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        t.join();
        EXPECT_TRUE(ran);
    }
}

/**
 * @test_case TC_MOCK_THREAD_CREATE
 * @tests REQ_PAL_THREAD_CREATE
 * @brief Thread runs callable immediately upon construction.
 */
TEST(ThreadConformance, CreateRunsCallable) {
    std::atomic<bool> ran{false};
    someip::platform::Thread t([&ran]() { ran = true; });
    t.join();
    EXPECT_TRUE(ran);
}

/**
 * @test_case TC_MOCK_THREAD_JOINABLE
 * @tests REQ_PAL_THREAD_JOINABLE
 * @brief joinable() returns true before join, false after.
 */
TEST(ThreadConformance, JoinableStatus) {
    std::atomic<bool> flag{false};
    someip::platform::Thread t([&flag]() { flag = true; });
    EXPECT_TRUE(t.joinable());
    t.join();
    EXPECT_FALSE(t.joinable());
}

/**
 * @test_case TC_MOCK_THREAD_JOIN
 * @tests REQ_PAL_THREAD_JOIN
 * @brief join() blocks until thread completes; idempotent.
 */
TEST(ThreadConformance, JoinIdempotent) {
    std::atomic<bool> ran{false};
    someip::platform::Thread t([&ran]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ran = true;
    });
    t.join();
    EXPECT_TRUE(ran);

    t.join();
    EXPECT_FALSE(t.joinable());
}

/**
 * @test_case TC_MOCK_THREAD_WITH_ARGS
 * @tests REQ_PAL_THREAD_CREATE
 * @brief Thread forwards arguments to callable.
 */
TEST(ThreadConformance, ThreadWithArguments) {
    std::atomic<int> result{0};
    someip::platform::Thread t([&result](int a, int b) {
        result = a + b;
    }, 40, 2);
    t.join();
    EXPECT_EQ(result.load(), 42);
}

/**
 * @test_case TC_MOCK_THREAD_NONCOPY
 * @tests REQ_PAL_THREAD_NONCOPY
 * @brief Thread is non-copyable and non-movable.
 */
TEST(ThreadConformance, NonCopyable) {
    EXPECT_FALSE(std::is_copy_constructible<someip::platform::Thread>::value);
    EXPECT_FALSE(std::is_copy_assignable<someip::platform::Thread>::value);
    EXPECT_FALSE(std::is_move_constructible<someip::platform::Thread>::value);
    EXPECT_FALSE(std::is_move_assignable<someip::platform::Thread>::value);
}

/**
 * @test_case TC_MOCK_THREAD_DTOR
 * @tests REQ_PAL_THREAD_DTOR_E01
 * @brief Destructor without join does not crash (short-lived task).
 */
TEST(ThreadConformance, DestructorWithoutJoinSafe) {
    {
        someip::platform::Thread t([]() {
            /* completes immediately */
        });
        // Brief sleep ensures the thread body finishes before destruction.
        // This test verifies that destroying an already-completed but
        // unjoined thread does not crash (REQ_PAL_THREAD_DTOR_E01).
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    SUCCEED();
}

/* ======================================================================== */
/*  ScopedLock Conformance                                                  */
/* ======================================================================== */

/**
 * @test_case TC_MOCK_LOCK_ACQUIRE
 * @tests REQ_PAL_LOCK_ACQUIRE
 * @brief ScopedLock constructor acquires the mutex.
 */
TEST(ScopedLockConformance, AcquireOnConstruction) {
    Mutex m;
    {
        ScopedLock guard(m);

        std::atomic<bool> got{false};
        std::thread probe([&] {
            got = m.try_lock();
            if (got) m.unlock();
        });
        probe.join();
        EXPECT_FALSE(got);
    }
}

/**
 * @test_case TC_MOCK_LOCK_RELEASE
 * @tests REQ_PAL_LOCK_RELEASE
 * @brief ScopedLock destructor releases the mutex.
 */
TEST(ScopedLockConformance, ReleaseOnDestruction) {
    Mutex m;
    { ScopedLock guard(m); }

    std::atomic<bool> got{false};
    std::thread probe([&] {
        got = m.try_lock();
        if (got) m.unlock();
    });
    probe.join();
    EXPECT_TRUE(got);
}

/**
 * @test_case TC_MOCK_LOCK_NONCOPY
 * @tests REQ_PAL_LOCK_NONCOPY
 * @brief ScopedLock is non-copyable.
 */
TEST(ScopedLockConformance, NonCopyable) {
    EXPECT_FALSE(std::is_copy_constructible<ScopedLock>::value);
    EXPECT_FALSE(std::is_copy_assignable<ScopedLock>::value);
}

/* ======================================================================== */
/*  sleep_for Conformance                                                   */
/* ======================================================================== */

/**
 * @test_case TC_MOCK_SLEEP_DURATION
 * @tests REQ_PAL_SLEEP_DURATION
 * @brief sleep_for blocks for at least the specified duration.
 */
TEST(SleepForConformance, MinimumDuration) {
    auto t0 = std::chrono::steady_clock::now();
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    EXPECT_GE(elapsed, 40);
    EXPECT_LE(elapsed, 500);
}

/**
 * @test_case TC_MOCK_SLEEP_ZERO
 * @tests REQ_PAL_SLEEP_ZERO
 * @brief sleep_for(0) returns immediately.
 */
TEST(SleepForConformance, ZeroDuration) {
    auto t0 = std::chrono::steady_clock::now();
    someip::platform::this_thread::sleep_for(std::chrono::milliseconds(0));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    EXPECT_LE(elapsed, 50);
}

/* ======================================================================== */
/*  Memory Conformance                                                      */
/* ======================================================================== */

/**
 * @test_case TC_MOCK_MEM_ALLOC
 * @tests REQ_PAL_MEM_ALLOC
 * @brief allocate_message() returns a usable non-null Message.
 */
TEST(MemoryConformance, AllocateReturnsUsable) {
    auto msg = someip::platform::allocate_message();
    ASSERT_NE(msg, nullptr);
    msg->set_service_id(0x1234);
    EXPECT_EQ(msg->get_service_id(), 0x1234);
}

/**
 * @test_case TC_MOCK_MEM_INDEPENDENT
 * @tests REQ_PAL_MEM_INDEPENDENT
 * @brief Multiple allocations return independent objects.
 */
TEST(MemoryConformance, IndependentAllocations) {
    auto a = someip::platform::allocate_message();
    auto b = someip::platform::allocate_message();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    a->set_service_id(0x1111);
    b->set_service_id(0x2222);

    EXPECT_EQ(a->get_service_id(), 0x1111);
    EXPECT_EQ(b->get_service_id(), 0x2222);
    EXPECT_NE(a.get(), b.get());
}
