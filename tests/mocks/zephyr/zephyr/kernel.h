/*
 * Mock Zephyr kernel API for host-based PAL conformance testing.
 * Implements k_mutex, k_condvar, k_thread using std primitives.
 */

#ifndef MOCK_ZEPHYR_KERNEL_H
#define MOCK_ZEPHYR_KERNEL_H

#include <cstdint>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>
#include <cassert>
#include <atomic>

#define __ASSERT(cond, ...) assert(cond)

/* ---- Timeout ---- */

struct k_timeout_t {
    int64_t ticks;
};

inline constexpr k_timeout_t K_FOREVER{-1};
inline constexpr k_timeout_t K_NO_WAIT{0};

#define K_PRIO_PREEMPT(x) (x)

/* ---- Stack ---- */

typedef char k_thread_stack_t;
#define K_KERNEL_STACK_MEMBER(name, size) k_thread_stack_t name[size]
#define K_THREAD_STACK_SIZEOF(stack) sizeof(stack)

/* ---- k_mutex ---- */

struct k_mutex {
    std::mutex mtx;
};

inline void k_mutex_init(k_mutex* m) {
    (void)m;
}

inline int k_mutex_lock(k_mutex* m, k_timeout_t timeout) {
    if (timeout.ticks == 0) {
        return m->mtx.try_lock() ? 0 : -11 /* -EAGAIN */;
    }
    m->mtx.lock();
    return 0;
}

inline int k_mutex_unlock(k_mutex* m) {
    m->mtx.unlock();
    return 0;
}

/* ---- k_condvar ---- */

struct k_condvar {
    std::condition_variable_any cv;
};

inline void k_condvar_init(k_condvar* cv) {
    (void)cv;
}

inline void k_condvar_signal(k_condvar* cv) {
    cv->cv.notify_one();
}

inline void k_condvar_broadcast(k_condvar* cv) {
    cv->cv.notify_all();
}

inline int k_condvar_wait(k_condvar* cv, k_mutex* m, k_timeout_t /*timeout*/) {
    cv->cv.wait(m->mtx);
    return 0;
}

/* ---- k_thread ---- */

typedef void (*k_thread_entry_t)(void*, void*, void*);
typedef struct k_thread* k_tid_t;

struct k_thread {
    std::thread impl_thread;
    bool started{false};
    bool joined{false};
    std::atomic<bool> stop_requested{false};
};

inline k_tid_t k_thread_create(
        k_thread* new_thread,
        k_thread_stack_t* /*stack*/,
        size_t /*stack_size*/,
        k_thread_entry_t entry,
        void* p1, void* p2, void* p3,
        int /*prio*/, uint32_t /*options*/, k_timeout_t /*delay*/)
{
    new_thread->impl_thread = std::thread([entry, p1, p2, p3]() {
        entry(p1, p2, p3);
    });
    new_thread->started = true;
    return new_thread;
}

inline int k_thread_join(k_thread* thread, k_timeout_t /*timeout*/) {
    if (thread->impl_thread.joinable()) {
        thread->impl_thread.join();
    }
    thread->joined = true;
    return 0;
}

// Test entry functions should receive the k_thread* via p1/p2/p3 and
// periodically check stop_requested to cooperate with cancellation.
inline void k_thread_abort(k_thread* thread) {
    thread->stop_requested = true;
    if (thread->impl_thread.joinable()) {
        // Give the thread a bounded window to finish cooperatively.
        // std::thread has no timed join, so we sleep then attempt join().
        // PAL conformance test entries are short-lived and should finish
        // well within this window once stop_requested is set.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (thread->impl_thread.joinable()) {
            thread->impl_thread.join();
        }
    }
    thread->started = false;
}

/* ---- k_msleep ---- */

inline int32_t k_msleep(int32_t ms) {
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return 0;
}

#endif /* MOCK_ZEPHYR_KERNEL_H */
