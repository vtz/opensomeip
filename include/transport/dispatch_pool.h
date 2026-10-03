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

#ifndef SOMEIP_TRANSPORT_DISPATCH_POOL_H
#define SOMEIP_TRANSPORT_DISPATCH_POOL_H

/**
 * @brief Optional worker pool that moves listener work off the receive thread.
 *
 * Transports deliver `ITransportListener` callbacks on their receive thread.
 * A pool is not installed by default, so that path stays synchronous and
 * RTOS ports do not gain threads. `RpcServer::set_message_dispatcher()` opts
 * in for method handlers.
 *
 * Worker count:
 * - 0: `submit()` runs the task on the caller before returning. No pool thread.
 * - 1: one FIFO worker. Every task is globally ordered. One extra thread.
 * - N (2..kMaxWorkers): task key selects one worker (`key % N`). Tasks that
 *   share a key stay FIFO on that worker. Different keys may run at the same
 *   time and are not ordered against each other. A handler blocked on one
 *   worker does not stall tasks assigned to another worker.
 *
 * `key_for()` mixes the sender endpoint (address, port, protocol), the
 * SOME/IP client id, and the session id. The same triple always maps to the
 * same worker, which is the per-client/per-session ordering guarantee.
 * SOME/IP clients normally increment the session id on every request, so two
 * in-flight requests from one client are different keys and may run in
 * parallel when N > 1. Use a pool of one worker when every request must stay
 * in a single global order (the same order as today's receive thread).
 *
 * Lifetime, matching `ITransport::set_listener()`:
 * - `start()` must succeed before `submit()`. `stop()` and the destructor
 *   reject new work, destroy tasks still queued (they do not run), and wait
 *   until any task already running returns.
 * - After `stop()` returns to a thread that is not itself running a pool
 *   task, no pool task runs. Calling `stop()` from a task does not deadlock:
 *   that task finishes, queued tasks are dropped, and other workers are
 *   joined. The calling worker is joined by a later `stop()` or by the
 *   destructor on a different thread. Do not destroy the pool on a worker.
 * - The pool does not outlive in-flight tasks. Destroy it only after `stop()`
 *   has returned on a non-worker thread, or let the destructor call `stop()`.
 * - Each worker queue holds `kQueueCapacity` tasks. `submit()` does not block
 *   the caller; a full queue returns false and drops that task.
 * - Task destructors run while a pool mutex may be held (queue drop on
 *   `stop()`). They must not call `submit()` or `stop()`.
 *
 * Threads are `platform::Thread` / `platform::Mutex` /
 * `platform::ConditionVariable`. The object reserves storage for
 * `kMaxWorkers` workers even when fewer are started; threads exist only
 * after a successful `start()` with a count of at least 1.
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include "platform/thread.h"
#include "transport/endpoint.h"

namespace someip::transport {

class DispatchWorkerPool {
public:
    static constexpr std::size_t kMaxWorkers = 8;
    static constexpr std::size_t kQueueCapacity = 32;
    static constexpr std::size_t kTaskStorage = 256;

    /**
     * @brief Construct a pool that will use @p worker_count workers.
     *
     * Does not spawn threads. Call `start()`. Counts above `kMaxWorkers`
     * fail in `start()`.
     */
    explicit DispatchWorkerPool(std::size_t worker_count);

    ~DispatchWorkerPool();

    DispatchWorkerPool(const DispatchWorkerPool&) = delete;
    DispatchWorkerPool& operator=(const DispatchWorkerPool&) = delete;
    DispatchWorkerPool(DispatchWorkerPool&&) = delete;
    DispatchWorkerPool& operator=(DispatchWorkerPool&&) = delete;

    /** @brief Configured worker count, including 0. Not the number running. */
    std::size_t worker_count() const {
        return worker_count_;
    }

    /**
     * @brief Spawn workers, or arm inline mode when the count is 0.
     * @return false if the count exceeds `kMaxWorkers`, a thread fails to
     *         start, or `stop()` has already been called.
     */
    [[nodiscard]] bool start();

    /**
     * @brief Drop queued tasks, wait for the task in progress, join workers.
     *
     * Safe to call from a pool task (does not join the calling worker).
     * Idempotent.
     */
    void stop();

    /**
     * @brief Queue @p fn on the worker selected by @p key, or run it inline.
     *
     * @return false if the pool was not started, is stopped, or the target
     *         queue is full. The task is not run in those cases.
     */
    template <typename Fn>
    [[nodiscard]] bool submit(std::size_t key, Fn&& fn) {
        using Task = std::decay_t<Fn>;
        static_assert(sizeof(Task) <= kTaskStorage,
                      "dispatch task exceeds DispatchWorkerPool storage");
        static_assert(alignof(Task) <= alignof(std::max_align_t),
                      "dispatch task alignment exceeds DispatchWorkerPool storage");
        static_assert(std::is_nothrow_move_constructible<Task>::value,
                      "dispatch task must be nothrow move constructible");
        DispatchTask task;
        DispatchTask::emplace(task, std::forward<Fn>(fn));
        return enqueue(key, std::move(task));
    }

    /**
     * @brief Sticky key for one sender, client, and session.
     *
     * Equal inputs produce an equal key in-process. With N > 1 the worker is
     * `key % N`. With N == 1 every key shares the single FIFO.
     */
    static std::size_t key_for(const Endpoint& sender, uint16_t client_id, uint16_t session_id);

    /**
     * @brief True when the caller is inside a task on this pool.
     *
     * Intended for shutdown: `stop()` must not join the calling worker.
     */
    bool running_on_worker() const;

private:
    class DispatchTask {
    public:
        DispatchTask() = default;
        ~DispatchTask() {
            reset();
        }

        DispatchTask(const DispatchTask&) = delete;
        DispatchTask& operator=(const DispatchTask&) = delete;

        DispatchTask(DispatchTask&& other) noexcept {
            move_from(other);
        }

        DispatchTask& operator=(DispatchTask&& other) noexcept {
            if (this != &other) {
                reset();
                move_from(other);
            }
            return *this;
        }

        template <typename Fn>
        static void emplace(DispatchTask& slot, Fn&& fn) {
            using Task = std::decay_t<Fn>;
            slot.reset();
            new (slot.storage_) Task(std::forward<Fn>(fn));
            slot.invoke_ = &invoke_fn<Task>;
            slot.move_ = &move_fn<Task>;
            slot.destroy_ = &destroy_fn<Task>;
            slot.engaged_ = true;
        }

        void operator()() {
            if (invoke_ != nullptr) {
                invoke_(storage_);
            }
        }

        bool engaged() const {
            return engaged_;
        }

    private:
        template <typename Task>
        static void invoke_fn(void* storage) {
            (*static_cast<Task*>(storage))();
        }

        template <typename Task>
        static void destroy_fn(void* storage) {
            static_cast<Task*>(storage)->~Task();
        }

        template <typename Task>
        static void move_fn(void* dst, void* src) {
            auto* source = static_cast<Task*>(src);
            new (dst) Task(std::move(*source));
            source->~Task();
        }

        void reset() noexcept {
            if (engaged_ && destroy_ != nullptr) {
                destroy_(storage_);
            }
            engaged_ = false;
            invoke_ = nullptr;
            move_ = nullptr;
            destroy_ = nullptr;
        }

        void move_from(DispatchTask& other) noexcept {
            if (!other.engaged_ || other.move_ == nullptr) {
                return;
            }
            other.move_(storage_, other.storage_);
            invoke_ = other.invoke_;
            move_ = other.move_;
            destroy_ = other.destroy_;
            engaged_ = true;
            other.engaged_ = false;
            other.invoke_ = nullptr;
            other.move_ = nullptr;
            other.destroy_ = nullptr;
        }

        alignas(std::max_align_t) unsigned char storage_[kTaskStorage]{};
        void (*invoke_)(void*){nullptr};
        void (*move_)(void*, void*){nullptr};
        void (*destroy_)(void*){nullptr};
        bool engaged_{false};
    };

    struct TaskQueue {
        bool push(DispatchTask&& task) {
            if (size_ >= kQueueCapacity) {
                return false;
            }
            slots_[tail_] = std::move(task);
            tail_ =
                static_cast<std::uint16_t>((static_cast<std::size_t>(tail_) + 1U) % kQueueCapacity);
            ++size_;
            return true;
        }

        DispatchTask pop() {
            DispatchTask task = std::move(slots_[head_]);
            head_ =
                static_cast<std::uint16_t>((static_cast<std::size_t>(head_) + 1U) % kQueueCapacity);
            --size_;
            return task;
        }

        void clear() {
            while (size_ > 0) {
                DispatchTask dropped = pop();
                (void)dropped;
            }
        }

        std::uint16_t size() const {
            return size_;
        }

        DispatchTask slots_[kQueueCapacity];
        std::uint16_t head_{0};
        std::uint16_t tail_{0};
        std::uint16_t size_{0};
    };

    struct WorkerSlot {
        DispatchWorkerPool* pool{nullptr};
        std::size_t index{0};
        void run();
    };

    bool enqueue(std::size_t key, DispatchTask task);
    void worker_loop(std::size_t index);
    void join_workers();

    const std::size_t worker_count_;
    bool started_{false};   ///< Guarded by mutex_.
    bool stopping_{false};  ///< Guarded by mutex_.
    platform::Mutex mutex_;
    platform::ConditionVariable cv_[kMaxWorkers];
    TaskQueue queues_[kMaxWorkers];
    WorkerSlot slots_[kMaxWorkers];
    std::optional<platform::Thread> threads_[kMaxWorkers];
    std::atomic<bool> join_claimed_[kMaxWorkers]{};
};

}  // namespace someip::transport

#endif  // SOMEIP_TRANSPORT_DISPATCH_POOL_H
