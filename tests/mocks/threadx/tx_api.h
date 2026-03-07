/*
 * Mock ThreadX API for host-based PAL conformance testing.
 * Implements TX_MUTEX, TX_EVENT_FLAGS_GROUP, tx_thread_create, etc.
 * using std primitives so the ThreadX PAL backend compiles and runs on host.
 */

#ifndef MOCK_TX_API_H
#define MOCK_TX_API_H

#include <cstdint>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cassert>

typedef unsigned int  UINT;
typedef unsigned long ULONG;
typedef char          CHAR;
typedef unsigned char UCHAR;

#define TX_SUCCESS        0u
#define TX_NOT_AVAILABLE  0x1Du
#define TX_INHERIT        1u
#define TX_WAIT_FOREVER   ((ULONG)0xFFFFFFFF)
#define TX_NO_WAIT        0u
#define TX_NO_TIME_SLICE  0u
#define TX_AUTO_START     1u
#define TX_OR             0u
#define TX_OR_CLEAR       1u

#ifndef TX_TIMER_TICKS_PER_SECOND
#define TX_TIMER_TICKS_PER_SECOND 1000u
#endif

/* ---- TX_MUTEX ---- */

struct TX_MUTEX {
    std::mutex mtx;
    bool created{false};
};

inline UINT tx_mutex_create(TX_MUTEX* m, CHAR* /*name*/, UINT /*inherit*/) {
    m->created = true;
    return TX_SUCCESS;
}

inline UINT tx_mutex_get(TX_MUTEX* m, ULONG wait) {
    if (wait == TX_NO_WAIT) {
        return m->mtx.try_lock() ? TX_SUCCESS : TX_NOT_AVAILABLE;
    }
    m->mtx.lock();
    return TX_SUCCESS;
}

inline UINT tx_mutex_put(TX_MUTEX* m) {
    m->mtx.unlock();
    return TX_SUCCESS;
}

inline UINT tx_mutex_delete(TX_MUTEX* m) {
    m->created = false;
    return TX_SUCCESS;
}

/* ---- TX_EVENT_FLAGS_GROUP ---- */

/*
 * Real ThreadX wakes ALL matching threads atomically when flags are set,
 * then clears.  We simulate this with a generation counter: each set()
 * increments the generation and wakes everyone.  Waiters record the
 * generation at entry and sleep until a newer generation arrives.  The
 * PAL's predicate-based CV loop handles the case where a thread wakes
 * but the higher-level condition isn't met.
 */
struct TX_EVENT_FLAGS_GROUP {
    std::mutex mtx;
    std::condition_variable cv;
    ULONG flags{0};
    uint64_t gen{0};
    bool created{false};
};

inline UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP* g, CHAR* /*name*/) {
    g->flags = 0;
    g->gen = 0;
    g->created = true;
    return TX_SUCCESS;
}

inline UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP* g,
                               ULONG flags_to_set, UINT /*set_option*/) {
    {
        std::lock_guard<std::mutex> lk(g->mtx);
        g->flags |= flags_to_set;
        ++g->gen;
    }
    g->cv.notify_all();
    return TX_SUCCESS;
}

inline UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP* g,
                               ULONG requested, UINT get_option,
                               ULONG* actual, ULONG wait) {
    std::unique_lock<std::mutex> lk(g->mtx);
    if (wait == TX_NO_WAIT) {
        if (g->flags & requested) {
            *actual = g->flags & requested;
            if (get_option == TX_OR_CLEAR) g->flags &= ~requested;
            return TX_SUCCESS;
        }
        return TX_NOT_AVAILABLE;
    }
    uint64_t my_gen = g->gen;
    g->cv.wait(lk, [&] { return g->gen > my_gen; });
    *actual = requested;
    (void)get_option;
    return TX_SUCCESS;
}

inline UINT tx_event_flags_delete(TX_EVENT_FLAGS_GROUP* g) {
    g->created = false;
    return TX_SUCCESS;
}

/* ---- TX_THREAD ---- */

typedef void (*tx_thread_entry_t)(ULONG);

struct TX_THREAD {
    std::thread thread;
    bool created{false};
};

inline UINT tx_thread_create(
        TX_THREAD* tcb,
        CHAR* /*name*/,
        tx_thread_entry_t entry,
        ULONG entry_input,
        void* /*stack_start*/,
        ULONG /*stack_size*/,
        UINT  /*priority*/,
        UINT  /*preempt_threshold*/,
        ULONG /*time_slice*/,
        UINT  /*auto_start*/)
{
    tcb->thread = std::thread([entry, entry_input]() {
        entry(entry_input);
    });
    tcb->thread.detach();
    tcb->created = true;
    return TX_SUCCESS;
}

inline UINT tx_thread_terminate(TX_THREAD* /*tcb*/) {
    return TX_SUCCESS;
}

inline UINT tx_thread_delete(TX_THREAD* tcb) {
    tcb->created = false;
    return TX_SUCCESS;
}

inline UINT tx_thread_sleep(ULONG ticks) {
    auto ms = (ticks * 1000ULL) / TX_TIMER_TICKS_PER_SECOND;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return TX_SUCCESS;
}

#endif /* MOCK_TX_API_H */
