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

#include "transport/dispatch_pool.h"

#include "platform/thread.h"
#include "transport/endpoint.h"

namespace someip::transport {

namespace {

thread_local const DispatchWorkerPool* tls_pool = nullptr;
thread_local std::size_t tls_worker_index = 0;

void mix_hash(std::size_t& hash, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32U; shift += 8U) {
        hash ^= static_cast<unsigned char>((value >> shift) & 0xFFu);
        hash *= 16777619u;
    }
}

}  // namespace

DispatchWorkerPool::DispatchWorkerPool(std::size_t worker_count) : worker_count_(worker_count) {
}

DispatchWorkerPool::~DispatchWorkerPool() {
    stop();
}

bool DispatchWorkerPool::start() {
    if (worker_count_ > kMaxWorkers) {
        return false;
    }

    {
        platform::ScopedLock const lock(mutex_);
        if (stopping_) {
            return false;
        }
        if (started_) {
            return true;
        }
        started_ = true;
        if (worker_count_ == 0) {
            return true;
        }
    }

    for (std::size_t i = 0; i < worker_count_; ++i) {
        slots_[i].pool = this;
        slots_[i].index = i;
        threads_[i].emplace(&WorkerSlot::run, &slots_[i]);
        if (!threads_[i]->started()) {
            stop();
            return false;
        }
    }
    return true;
}

void DispatchWorkerPool::stop() {
    {
        platform::ScopedLock const lock(mutex_);
        stopping_ = true;
        const std::size_t limit = worker_count_ < kMaxWorkers ? worker_count_ : kMaxWorkers;
        for (std::size_t i = 0; i < limit; ++i) {
            queues_[i].clear();
        }
    }

    const std::size_t limit = worker_count_ < kMaxWorkers ? worker_count_ : kMaxWorkers;
    for (std::size_t i = 0; i < limit; ++i) {
        cv_[i].notify_one();
    }
    join_workers();
}

bool DispatchWorkerPool::enqueue(std::size_t key, DispatchTask task) {
    if (worker_count_ == 0) {
        {
            platform::ScopedLock const lock(mutex_);
            if (!started_ || stopping_) {
                return false;
            }
        }
        task();
        return true;
    }

    if (worker_count_ > kMaxWorkers) {
        return false;
    }

    const std::size_t index = key % worker_count_;
    {
        platform::ScopedLock const lock(mutex_);
        if (!started_ || stopping_) {
            return false;
        }
        if (!queues_[index].push(std::move(task))) {
            return false;
        }
    }
    cv_[index].notify_one();
    return true;
}

std::size_t DispatchWorkerPool::key_for(const Endpoint& sender, uint16_t client_id,
                                        uint16_t session_id) {
    // FNV-1a. Session id is part of the key on purpose: equal
    // (endpoint, client, session) values stay on one worker. Distinct
    // sessions from the same client may run concurrently when N > 1.
    std::size_t hash = 2166136261u;
    const platform::String<>& address = sender.get_address();
    for (std::size_t i = 0; i < address.size(); ++i) {
        hash ^= static_cast<unsigned char>(address[i]);
        hash *= 16777619u;
    }
    mix_hash(hash, sender.get_port());
    mix_hash(hash, static_cast<std::uint32_t>(sender.get_protocol()));
    mix_hash(hash, client_id);
    mix_hash(hash, session_id);
    return hash;
}

bool DispatchWorkerPool::running_on_worker() const {
    return tls_pool == this;
}

void DispatchWorkerPool::WorkerSlot::run() {
    tls_pool = pool;
    tls_worker_index = index;
    if (pool != nullptr) {
        pool->worker_loop(index);
    }
    if (tls_pool == pool) {
        tls_pool = nullptr;
    }
}

void DispatchWorkerPool::worker_loop(std::size_t index) {
    for (;;) {
        DispatchTask task;
        {
            platform::ScopedLock const lock(mutex_);
            cv_[index].wait(mutex_,
                            [this, index]() { return stopping_ || queues_[index].size() > 0; });
            if (stopping_) {
                return;
            }
            task = queues_[index].pop();
        }
        task();
    }
}

void DispatchWorkerPool::join_workers() {
    // One joiner per worker. stop() from a task must not take a lock that an
    // external stop() holds while it joins that task's thread.
    const bool self = running_on_worker();
    const std::size_t limit = worker_count_ < kMaxWorkers ? worker_count_ : kMaxWorkers;
    for (std::size_t i = 0; i < limit; ++i) {
        if (self && tls_worker_index == i) {
            continue;
        }
        bool expected = false;
        if (!join_claimed_[i].compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            continue;
        }
        if (threads_[i].has_value() && threads_[i]->joinable()) {
            threads_[i]->join();
        }
        threads_[i].reset();
    }
}

}  // namespace someip::transport
