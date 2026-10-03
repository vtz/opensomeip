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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

#include "static_pool_init.h"
#include "transport/dispatch_pool.h"
#include "transport/endpoint.h"

using someip::transport::DispatchWorkerPool;
using someip::transport::Endpoint;
using someip::transport::TransportProtocol;

namespace {

bool wait_until(const std::atomic<bool>& flag, int timeout_ms = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!flag.load()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

}  // namespace

/**
 * @brief Worker count 0 runs the task inline on the caller.
 */
TEST(DispatchWorkerPoolTest, SizeZeroDeliversInlineOnCaller) {
    DispatchWorkerPool pool(0);
    std::atomic<int> runs{0};
    EXPECT_FALSE(pool.submit(0, [&] { runs.store(1); }));
    EXPECT_EQ(runs.load(), 0);

    ASSERT_TRUE(pool.start());
    const auto caller = std::this_thread::get_id();
    std::thread::id seen;
    ASSERT_TRUE(pool.submit(7, [&] {
        runs.store(2);
        seen = std::this_thread::get_id();
    }));
    EXPECT_EQ(runs.load(), 2);
    EXPECT_EQ(seen, caller);

    pool.stop();
    EXPECT_FALSE(pool.submit(0, [&] { runs.store(3); }));
    EXPECT_EQ(runs.load(), 2);
}

/**
 * @brief One worker is a global FIFO, including across different keys.
 */
TEST(DispatchWorkerPoolTest, SizeOneIsGloballyOrdered) {
    std::atomic<int> next{0};
    std::vector<int> order;
    order.reserve(5);
    std::mutex mutex;
    DispatchWorkerPool pool(1);
    ASSERT_TRUE(pool.start());

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(pool.submit(static_cast<std::size_t>(i * 3), [&, i] {
            std::lock_guard<std::mutex> lock(mutex);
            order.push_back(i);
            next.store(i + 1);
        }));
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (next.load() < 5 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(next.load(), 5);
    ASSERT_EQ(order.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(order[static_cast<std::size_t>(i)], i);
    }
    pool.stop();
}

/**
 * @brief A blocked handler does not stall a task hashed to another worker.
 */
TEST(DispatchWorkerPoolTest, DifferentKeysRunConcurrently) {
    std::atomic<bool> release{false};
    DispatchWorkerPool pool(2);
    struct Release {
        std::atomic<bool>& flag;
        ~Release() {
            flag.store(true);
        }
    } release_guard{release};

    ASSERT_TRUE(pool.start());
    std::atomic<bool> entered{false};
    std::atomic<bool> other_done{false};

    ASSERT_TRUE(pool.submit(0, [&] {
        entered.store(true);
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }));
    ASSERT_TRUE(wait_until(entered));
    ASSERT_TRUE(pool.submit(1, [&] { other_done.store(true); }));
    EXPECT_TRUE(wait_until(other_done));
    release.store(true);
    pool.stop();
}

/**
 * @brief Tasks with the same key stay in submit order on one worker.
 */
TEST(DispatchWorkerPoolTest, SameKeyStaysOrdered) {
    std::atomic<int> stage{0};
    std::vector<int> order;
    std::mutex mutex;
    DispatchWorkerPool pool(2);
    struct Unblock {
        std::atomic<int>& stage_flag;
        ~Unblock() {
            stage_flag.store(2);
        }
    } unblock{stage};

    ASSERT_TRUE(pool.start());
    constexpr std::size_t kKey = 3;
    ASSERT_EQ(kKey % 2U, 1U);

    ASSERT_TRUE(pool.submit(kKey, [&] {
        stage.store(1);
        while (stage.load() == 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::lock_guard<std::mutex> lock(mutex);
        order.push_back(1);
    }));
    const auto entered_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (stage.load() != 1 && std::chrono::steady_clock::now() < entered_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(stage.load(), 1);

    ASSERT_TRUE(pool.submit(kKey, [&] {
        std::lock_guard<std::mutex> lock(mutex);
        order.push_back(2);
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::lock_guard<std::mutex> lock(mutex);
        EXPECT_TRUE(order.empty());
    }

    stage.store(2);
    const auto done_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    bool done = false;
    while (std::chrono::steady_clock::now() < done_deadline) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            done = order.size() == 2U;
        }
        if (done) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(done);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    pool.stop();
}

/**
 * @brief stop() returns while a handler is in progress, once that handler returns,
 *        and does not run tasks that were only queued.
 */
TEST(DispatchWorkerPoolTest, StopDoesNotDeadlockWhileHandlerRuns) {
    std::atomic<bool> release{false};
    DispatchWorkerPool pool(2);
    struct Release {
        std::atomic<bool>& flag;
        ~Release() {
            flag.store(true);
        }
    } release_guard{release};

    ASSERT_TRUE(pool.start());
    std::atomic<bool> entered{false};
    std::atomic<int> runs{0};

    ASSERT_TRUE(pool.submit(0, [&] {
        entered.store(true);
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        runs.fetch_add(1);
    }));
    ASSERT_TRUE(wait_until(entered));
    ASSERT_TRUE(pool.submit(0, [&] { runs.fetch_add(10); }));

    std::atomic<bool> stopped{false};
    std::thread stopper([&] {
        pool.stop();
        stopped.store(true);
    });
    struct JoinStopper {
        std::thread& thread;
        std::atomic<bool>& release_flag;
        ~JoinStopper() {
            release_flag.store(true);
            if (thread.joinable()) {
                thread.join();
            }
        }
    } join_stopper{stopper, release};

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(stopped.load());
    release.store(true);
    ASSERT_TRUE(wait_until(stopped));
    EXPECT_EQ(runs.load(), 1);

    std::atomic<bool> late{false};
    EXPECT_FALSE(pool.submit(1, [&] { late.store(true); }));
    EXPECT_FALSE(late.load());
}

/**
 * @brief stop() from inside a running task returns without joining that worker.
 */
TEST(DispatchWorkerPoolTest, StopFromRunningTaskReturns) {
    DispatchWorkerPool pool(1);
    ASSERT_TRUE(pool.start());

    std::atomic<bool> stop_returned{false};
    ASSERT_TRUE(pool.submit(0, [&] {
        pool.stop();
        stop_returned.store(true);
    }));
    ASSERT_TRUE(wait_until(stop_returned));
    pool.stop();
}

/**
 * @brief The per-worker queue is bounded and does not block the caller.
 */
TEST(DispatchWorkerPoolTest, FullQueueRejectsWithoutBlocking) {
    std::atomic<bool> release{false};
    DispatchWorkerPool pool(1);
    struct Release {
        std::atomic<bool>& flag;
        ~Release() {
            flag.store(true);
        }
    } release_guard{release};

    ASSERT_TRUE(pool.start());
    std::atomic<bool> entered{false};
    ASSERT_TRUE(pool.submit(0, [&] {
        entered.store(true);
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }));
    ASSERT_TRUE(wait_until(entered));

    int accepted = 0;
    for (int i = 0; i < 64; ++i) {
        if (pool.submit(0, [] {})) {
            ++accepted;
        }
    }
    EXPECT_EQ(accepted, static_cast<int>(DispatchWorkerPool::kQueueCapacity));
    release.store(true);
    pool.stop();
}

/**
 * @brief key_for is stable and includes the session id.
 */
TEST(DispatchWorkerPoolTest, KeyForSticksToEndpointClientAndSession) {
    const Endpoint sender("127.0.0.1", 40000, TransportProtocol::UDP);
    const Endpoint other_port("127.0.0.1", 40001, TransportProtocol::UDP);
    const std::size_t first = DispatchWorkerPool::key_for(sender, 0x10, 0x01);
    EXPECT_EQ(first, DispatchWorkerPool::key_for(sender, 0x10, 0x01));
    EXPECT_NE(first, DispatchWorkerPool::key_for(sender, 0x10, 0x02));
    EXPECT_NE(first, DispatchWorkerPool::key_for(sender, 0x11, 0x01));
    EXPECT_NE(first, DispatchWorkerPool::key_for(other_port, 0x10, 0x01));
}

TEST(DispatchWorkerPoolTest, TooManyWorkersFailsStart) {
    DispatchWorkerPool pool(DispatchWorkerPool::kMaxWorkers + 1);
    EXPECT_FALSE(pool.start());
    EXPECT_FALSE(pool.submit(0, [] {}));
}
