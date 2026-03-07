/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_THREADX_THREAD_IMPL_H
#define SOMEIP_PLATFORM_THREADX_THREAD_IMPL_H

/**
 * @brief ThreadX threading backend.
 *
 * Provides Mutex, ConditionVariable, Thread, and sleep_for using
 * ThreadX primitives.
 *
 * Mutex uses TX_MUTEX with TX_INHERIT for priority inheritance.
 *
 * ConditionVariable uses TX_EVENT_FLAGS_GROUP with a single flag bit.
 * notify_one() sets the flag; wait() clears it on receipt.  Callers
 * must always use the predicate form to handle races where the flag
 * was set before the waiter entered wait().
 *
 * Thread uses tx_thread_create() with a static stack buffer.  A
 * TX_EVENT_FLAGS_GROUP is used for join() synchronization.
 * The trampoline signature is void(*)(ULONG), matching the ThreadX
 * thread entry prototype.
 *
 * sleep_for converts std::chrono durations to ThreadX ticks using
 * TX_TIMER_TICKS_PER_SECOND.
 */

#include <tx_api.h>

#include <atomic>
#include <cstdlib>
#include <functional>
#include <chrono>
#include <cstdint>
#include <tuple>

#ifndef SOMEIP_THREADX_THREAD_STACK_SIZE
#define SOMEIP_THREADX_THREAD_STACK_SIZE 4096
#endif

#ifndef SOMEIP_THREADX_THREAD_PRIORITY
#define SOMEIP_THREADX_THREAD_PRIORITY 16
#endif

namespace someip {
namespace platform {

/** @implements REQ_PLATFORM_THREADX_001, REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_MUTEX_NONCOPY, REQ_PAL_MUTEX_UNLOCK_E01 */
class Mutex {
public:
    Mutex() {
        tx_mutex_create(&mtx_, const_cast<CHAR*>("someip_mtx"), TX_INHERIT);
    }

    ~Mutex() {
        tx_mutex_delete(&mtx_);
    }

    /** @implements REQ_PAL_MUTEX_LOCK */
    void lock() {
        tx_mutex_get(&mtx_, TX_WAIT_FOREVER);
    }

    /** @implements REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_UNLOCK_E01 */
    void unlock() {
        tx_mutex_put(&mtx_);
    }

    /** @implements REQ_PAL_MUTEX_TRYLOCK */
    bool try_lock() {
        return tx_mutex_get(&mtx_, TX_NO_WAIT) == TX_SUCCESS;
    }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

private:
    TX_MUTEX mtx_;
};

/** @implements REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL, REQ_PAL_CV_OWNERSHIP */
class ConditionVariable {
public:
    ConditionVariable() {
        tx_event_flags_create(&ev_, const_cast<CHAR*>("someip_cv"));
    }

    ~ConditionVariable() {
        tx_event_flags_delete(&ev_);
    }

    /** @implements REQ_PAL_CV_NOTIFY_ONE */
    void notify_one() {
        tx_event_flags_set(&ev_, 0x1, TX_OR);
    }

    /** @implements REQ_PAL_CV_NOTIFY_ALL */
    void notify_all() {
        tx_event_flags_set(&ev_, 0x1, TX_OR);
    }

    /** @implements REQ_PAL_CV_WAIT, REQ_PAL_CV_OWNERSHIP */
    void wait(Mutex& mtx) {
        ULONG actual = 0;
        mtx.unlock();
        tx_event_flags_get(&ev_, 0x1, TX_OR_CLEAR, &actual, TX_WAIT_FOREVER);
        mtx.lock();
    }

    /** @implements REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_OWNERSHIP */
    template <typename Pred>
    void wait(Mutex& mtx, Pred pred) {
        while (!pred()) {
            wait(mtx);
        }
    }

    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

private:
    TX_EVENT_FLAGS_GROUP ev_;
};

/** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_THREAD_NONCOPY, REQ_PAL_THREAD_CREATE_E01, REQ_PAL_THREAD_DTOR_E01 */
class Thread {
public:
    Thread() = default;

    /** @implements REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_CREATE_E01 */
    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args) {
        tx_event_flags_create(&join_ev_, const_cast<CHAR*>("someip_join"));

        ctx_ = new std::function<void()>(
            [f = std::forward<Fn>(fn),
             a = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                std::apply(std::move(f), std::move(a));
            });

        // tx_thread_create's entry_input is ULONG (unsigned int on the
        // ThreadX Linux port), which is too narrow to hold a pointer on
        // 64-bit hosts.  Pass an integer slot index into a global registry
        // instead of casting this directly to ULONG.
        slot_ = alloc_slot(this);
        if (slot_ == kInvalidSlot) {
            delete ctx_;
            ctx_ = nullptr;
            tx_event_flags_delete(&join_ev_);
            return;
        }

        UINT rc = tx_thread_create(
            &tcb_,
            const_cast<CHAR*>("someip"),
            trampoline,
            slot_,
            stack_,
            sizeof(stack_),
            SOMEIP_THREADX_THREAD_PRIORITY,
            SOMEIP_THREADX_THREAD_PRIORITY,
            TX_NO_TIME_SLICE,
            TX_AUTO_START);

        if (rc != TX_SUCCESS) {
            release_slot_if_owner();
            delete ctx_;
            ctx_ = nullptr;
            tx_event_flags_delete(&join_ev_);
            return;
        }
        started_ = true;
    }

    /** @implements REQ_PAL_THREAD_DTOR_E01 */
    ~Thread() {
        if (joinable()) {
            tx_thread_terminate(&tcb_);
            // The trampoline may have been scheduled but not yet run when
            // tx_thread_terminate killed it.  Clear the registry slot so it
            // is not leaked; use CAS so we do not double-free if the
            // trampoline already claimed and cleared the slot.
            release_slot_if_owner();
            tx_thread_delete(&tcb_);
            delete ctx_;
            ctx_ = nullptr;
            tx_event_flags_delete(&join_ev_);
            started_ = false;
        }
    }

    /** @implements REQ_PAL_THREAD_JOINABLE */
    bool joinable() const { return started_ && !joined_; }

    /** @implements REQ_PAL_THREAD_JOIN */
    void join() {
        if (!joinable()) return;
        ULONG actual = 0;
        tx_event_flags_get(&join_ev_, 0x1, TX_OR_CLEAR, &actual, TX_WAIT_FOREVER);
        // trampoline has completed and already released the slot via CAS;
        // release_slot_if_owner() here is a no-op but keeps ownership clear.
        release_slot_if_owner();
        joined_ = true;
        tx_thread_delete(&tcb_);
        delete ctx_;
        ctx_ = nullptr;
        tx_event_flags_delete(&join_ev_);
    }

    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

private:
    // Global registry: passes Thread* to the trampoline via an integer slot
    // index, sidestepping ULONG being too narrow for pointers on 64-bit hosts.
    static constexpr ULONG kMaxSlots   = 64;
    static constexpr ULONG kInvalidSlot = kMaxSlots; // sentinel: no slot held
    inline static std::atomic<Thread*> s_registry[kMaxSlots] = {};

    // Returns kInvalidSlot if no slot is available (instead of aborting).
    static ULONG alloc_slot(Thread* t) {
        for (ULONG i = 0; i < kMaxSlots; ++i) {
            Thread* expected = nullptr;
            if (s_registry[i].compare_exchange_strong(
                    expected, t,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                return i;
            }
        }
        return kInvalidSlot;
    }

    // CAS-based release: clears s_registry[slot_] only if it still points to
    // this Thread, then marks slot_ as invalid.  Safe to call from destructor,
    // join(), or trampoline() without risk of double-free or use-after-free.
    void release_slot_if_owner() {
        if (slot_ == kInvalidSlot) return;
        Thread* expected = this;
        s_registry[slot_].compare_exchange_strong(
            expected, nullptr,
            std::memory_order_acq_rel,
            std::memory_order_relaxed);
        slot_ = kInvalidSlot;
    }

    static void trampoline(ULONG slot) {
        Thread* self = s_registry[slot].load(std::memory_order_acquire);
        // CAS-release the slot: the destructor may have beaten us here if the
        // thread was terminated before the trampoline ran.  In that case self
        // is already gone; bail out to avoid a use-after-free.
        Thread* expected = self;
        if (!s_registry[slot].compare_exchange_strong(
                expected, nullptr,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            return; // destructor already owns/destroyed the Thread object
        }
        if (self) self->slot_ = kInvalidSlot;
        if (self && self->ctx_ && *(self->ctx_)) {
            (*(self->ctx_))();
        }
        if (self) tx_event_flags_set(&self->join_ev_, 0x1, TX_OR);
    }

    TX_THREAD tcb_{};
    TX_EVENT_FLAGS_GROUP join_ev_{};
    UCHAR stack_[SOMEIP_THREADX_THREAD_STACK_SIZE]{};
    std::function<void()>* ctx_{nullptr};
    ULONG slot_{kInvalidSlot};
    bool started_{false};
    bool joined_{false};
};

namespace this_thread {

/** @implements REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO */
template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& d) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    if (ms <= 0) return;
    ULONG ticks = static_cast<ULONG>(
        (static_cast<unsigned long long>(ms) * TX_TIMER_TICKS_PER_SECOND) / 1000ULL);
    if (ticks == 0) ticks = 1;
    tx_thread_sleep(ticks);
}

} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_THREADX_THREAD_IMPL_H
