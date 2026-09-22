/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
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
#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "../src/transport/transport_session.h"
#include "callback_release_guard.h"
#include "events/event_publisher.h"
#include "rpc/rpc_server.h"
#include "someip/message.h"
#include "static_pool_init.h"
#include "transport/transport.h"

/**
 * @test_case TC_FACADE_TEARDOWN
 * @tests REQ_ARCH_002
 */
namespace {

using namespace someip;
using transport::Endpoint;
using transport::ITransportListener;

constexpr uint16_t SERVICE = 0x2001;

/**
 * @brief Minimal fake ITransport that counts and observes concurrency of stop().
 *
 * `stop()` deliberately holds its "in progress" window open for a short time
 * so that, on the unfixed TransportSession (plain `bool active_`), two
 * concurrently racing callers are very likely to both observe `active_ ==
 * true` and both reach this method at the same time. `max_concurrent_stops`
 * records whether that ever happened.
 */
class FakeTransport final : public transport::ITransport {
   public:
    Result send_message(const Message&, const Endpoint&) override
    {
        return Result::SUCCESS;
    }

    MessagePtr receive_message() override
    {
        return nullptr;
    }

    Result connect(const Endpoint&) override
    {
        return Result::NOT_IMPLEMENTED;
    }

    Result disconnect() override
    {
        return Result::NOT_IMPLEMENTED;
    }

    bool is_connected() const override
    {
        return running_;
    }

    Endpoint get_local_endpoint() const override
    {
        return Endpoint("192.0.2.9", 31009);
    }

    void set_listener(ITransportListener* listener) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_ = listener;
    }

    Result start() override
    {
        ++starts;
        running_ = true;
        return start_result;
    }

    Result stop() override
    {
        const int concurrent = ++in_progress_;
        int observed_max = max_concurrent_stops.load();
        while (concurrent > observed_max &&
               !max_concurrent_stops.compare_exchange_weak(observed_max, concurrent)) {
        }
        // Widen the race window; the unfixed session lets a second racing
        // caller observe active_ == true and enter here concurrently.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ++stop_calls;
        if (on_stop) {
            on_stop();
        }
        running_ = remain_running_on_stop;
        --in_progress_;
        if (throw_on_stop) {
            throw std::runtime_error("FakeTransport::stop failed");
        }
        return stop_result;
    }

    bool is_running() const override
    {
        return running_;
    }

    ITransportListener* listener()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return listener_;
    }

    Result start_result{Result::SUCCESS};
    Result stop_result{Result::SUCCESS};
    bool remain_running_on_stop{false};
    bool throw_on_stop{false};
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> stop_calls{0};
    std::atomic<int> max_concurrent_stops{0};
    std::function<void()> on_stop;

   private:
    std::atomic<bool> running_{false};
    std::atomic<int> in_progress_{0};
    std::mutex mutex_;
    ITransportListener* listener_{nullptr};
};

struct ResetStopHook {
    FakeTransport& transport;
    ~ResetStopHook()
    {
        transport.on_stop = nullptr;
    }
};

/**
 * @test_case TC_FACADE_CONCURRENT_SHUTDOWN
 * @tests REQ_ARCH_002
 *
 * Two threads calling RpcServer::shutdown() concurrently must still stop the
 * borrowed transport exactly once, and must never invoke ITransport::stop()
 * re-entrantly. Before the fix, `TransportSession::active_` was a plain
 * bool: both shutdown() calls could see `!running_` false-then-true (or
 * both see running_ true), and both call transport_session_.stop(), both
 * observing active_ == true and both reaching transport_.stop().
 */
TEST(FacadeTeardownTest, ConcurrentShutdownCallsStopExactlyOnceOnRpcServer)
{
    FakeTransport transport;
    rpc::RpcServer server(SERVICE, transport);
    ASSERT_TRUE(server.initialize());

    std::thread t1([&] { server.shutdown(); });
    std::thread t2([&] { server.shutdown(); });
    t1.join();
    t2.join();

    EXPECT_EQ(transport.stop_calls, 1u);
    EXPECT_LE(transport.max_concurrent_stops.load(), 1);
    EXPECT_EQ(transport.listener(), nullptr);
    EXPECT_FALSE(transport.is_running());
}

/**
 * @test_case TC_RPC_SHUTDOWN_SERIALIZATION
 * @tests REQ_ARCH_002
 */
TEST(FacadeTeardownTest, ShutdownCallersWaitForTheCompleteTeardown)
{
    FakeTransport transport;
    rpc::RpcServer server(SERVICE, transport);
    ResetStopHook const reset_hook{transport};
    const auto handler = [](uint16_t, uint16_t, const platform::ByteBuffer&,
                            platform::ByteBuffer&) { return rpc::RpcResult::SUCCESS; };
    ASSERT_TRUE(server.register_method(1, handler));
    ASSERT_TRUE(server.initialize());
    std::promise<void> entered;
    std::promise<void> release;
    std::promise<void> second_entered;
    auto released = release.get_future().share();
    std::future<void> first;
    std::future<void> second;
    test::CallbackReleaseGuard release_guard(release);
    transport.on_stop = [&] {
        entered.set_value();
        EXPECT_EQ(released.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    };
    first = std::async(std::launch::async, [&] { server.shutdown(); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(2)), std::future_status::ready);
    second = std::async(std::launch::async, [&] {
        second_entered.set_value();
        server.shutdown();
    });
    ASSERT_EQ(second_entered.get_future().wait_for(std::chrono::seconds(2)),
              std::future_status::ready);
    EXPECT_EQ(second.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    EXPECT_FALSE(server.register_method(2, handler));
    release_guard.release();
    ASSERT_EQ(first.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    first.get();
    ASSERT_EQ(second.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    second.get();
    EXPECT_EQ(transport.stop_calls, 1u);
    EXPECT_TRUE(server.get_registered_methods().empty());
    EXPECT_TRUE(server.register_method(2, handler));
    transport.on_stop = nullptr;
}

TEST(FacadeTeardownTest, InitializationWaitsForShutdownCleanup)
{
    FakeTransport transport;
    rpc::RpcServer server(SERVICE, transport);
    ResetStopHook const reset_hook{transport};
    ASSERT_TRUE(server.initialize());
    std::promise<void> entered;
    std::promise<void> release;
    std::promise<void> initialize_entered;
    auto released = release.get_future().share();
    std::future<void> shutdown;
    std::future<bool> initialize;
    test::CallbackReleaseGuard release_guard(release);
    transport.on_stop = [&] {
        entered.set_value();
        EXPECT_EQ(released.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    };
    shutdown = std::async(std::launch::async, [&] { server.shutdown(); });
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds(2)), std::future_status::ready);
    initialize = std::async(std::launch::async, [&] {
        initialize_entered.set_value();
        return server.initialize();
    });
    ASSERT_EQ(initialize_entered.get_future().wait_for(std::chrono::seconds(2)),
              std::future_status::ready);
    EXPECT_EQ(initialize.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    release_guard.release();
    ASSERT_EQ(shutdown.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    shutdown.get();
    ASSERT_EQ(initialize.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_TRUE(initialize.get());
    EXPECT_EQ(transport.starts, 2u);
    transport.on_stop = nullptr;
}

#ifdef __cpp_exceptions
TEST(FacadeTeardownTest, ShutdownGateIsReleasedOnException)
{
    FakeTransport transport;
    rpc::RpcServer server(SERVICE, transport);
    ASSERT_TRUE(server.initialize());
    transport.throw_on_stop = true;
    EXPECT_THROW(server.shutdown(), std::runtime_error);
    EXPECT_TRUE(
        server.register_method(1, [](uint16_t, uint16_t, const platform::ByteBuffer&,
                                     platform::ByteBuffer&) { return rpc::RpcResult::SUCCESS; }));
    transport.throw_on_stop = false;
}
#endif

// EventPublisher also owns a cyclic-publish timer thread that shutdown()
// joins; concurrently racing EventPublisher::shutdown() from two threads can
// double-join that std::thread (a separate, pre-existing hazard outside the
// scope of the TransportSession::active_ fix), so the concurrent-shutdown
// regression below is exercised through RpcServer, which has no such thread.
// EventPublisher's exception-firewall destructor is still covered below by
// EventPublisherDestructorSwallowsThrowingTransportStop.

// A destructor genuinely racing an in-flight shutdown() call on the very
// same object (as opposed to two shutdown() calls, exercised above) requires
// some form of shared ownership (e.g. std::shared_ptr) to keep the object
// alive while the other thread is still inside a member call; without that,
// destroying the object concurrently is a use-after-free independent of the
// TransportSession fix and not something atomics can make well-defined. The
// TransportSession-level test below (ConcurrentStopClaimsOwnershipExactlyOnce)
// exercises the same underlying stop()-vs-stop() race that a destructor
// racing shutdown() would hit, without relying on unsafe object lifetime
// management in the test itself.

#ifdef __cpp_exceptions
/**
 * @test_case TC_FACADE_DESTRUCTOR_EXCEPTION_FIREWALL
 * @tests REQ_ARCH_002
 *
 * Destroying a facade whose injected transport throws from stop() must not
 * terminate the process. Before the fix, ~RpcServerImpl / ~EventPublisherImpl
 * were bare `{ shutdown(); }` (implicitly noexcept), so this would call
 * std::terminate.
 */
TEST(FacadeTeardownTest, RpcServerDestructorSwallowsThrowingTransportStop)
{
    FakeTransport transport;
    {
        rpc::RpcServer server(SERVICE, transport);
        ASSERT_TRUE(server.initialize());
        transport.throw_on_stop = true;
        // Destructor must not propagate/terminate even though transport.stop() throws.
    }
    SUCCEED();
}

TEST(FacadeTeardownTest, EventPublisherDestructorSwallowsThrowingTransportStop)
{
    FakeTransport transport;
    {
        events::EventPublisher publisher(SERVICE, 1, transport);
        ASSERT_TRUE(publisher.initialize());
        transport.throw_on_stop = true;
    }
    SUCCEED();
}

/**
 * @note The `!running_` retry branch in shutdown() also reaches
 *       transport_session_.stop() (e.g. when a prior stop() left the backend
 *       running and TransportSession re-armed for retry). Verify the
 *       destructor's exception firewall covers that path too, not just the
 *       "normal" running_ == true path.
 */
TEST(FacadeTeardownTest, RpcServerDestructorSwallowsThrowOnRetryPath)
{
    FakeTransport transport;
    {
        rpc::RpcServer server(SERVICE, transport);
        ASSERT_TRUE(server.initialize());
        // Leave the backend "running" so TransportSession re-arms active_ for
        // a retry; this first shutdown() call must not throw.
        transport.remain_running_on_stop = true;
        server.shutdown();
        // The destructor now takes the !running_ retry branch, and this time
        // the backend throws while trying again.
        transport.remain_running_on_stop = false;
        transport.throw_on_stop = true;
    }
    SUCCEED();
}
#endif  // __cpp_exceptions

/**
 * @test_case TC_TRANSPORT_SESSION_RETRY
 * @tests REQ_ARCH_002, REQ_ARCH_004
 *
 * Directly exercises TransportSession to confirm the atomic active_ change
 * preserved the documented retry semantics: if the backend is still running
 * after stop(), the session re-arms so a later shutdown()/stop() retries
 * cleanup instead of silently no-op'ing.
 */
TEST(TransportSessionDirectTest, ReArmsForRetryWhenBackendStaysRunning)
{
    FakeTransport transport;
    transport::detail::TransportSession session(transport);

    struct Listener : public ITransportListener {
        void on_message_received(MessagePtr, const Endpoint&) override
        {
        }
        void on_connection_lost(const Endpoint&) override
        {
        }
        void on_connection_established(const Endpoint&) override
        {
        }
        void on_error(Result) override
        {
        }
    } listener;

    ASSERT_EQ(session.start(listener), Result::SUCCESS);

    transport.remain_running_on_stop = true;
    EXPECT_EQ(session.stop(), Result::SUCCESS);
    // Backend still running -> session must have re-armed (retry path), so a
    // second stop() call must try again instead of being a silent no-op.
    EXPECT_EQ(transport.stop_calls, 1u);

    transport.remain_running_on_stop = false;
    EXPECT_EQ(session.stop(), Result::SUCCESS);
    EXPECT_EQ(transport.stop_calls, 2u);
    EXPECT_EQ(session.result(), Result::SUCCESS);

    // Session is now fully stopped: a further stop() must be a true no-op.
    EXPECT_EQ(session.stop(), Result::SUCCESS);
    EXPECT_EQ(transport.stop_calls, 2u);
}

/**
 * @test_case TC_TRANSPORT_SESSION_CONCURRENT_STOP
 * @tests REQ_ARCH_002
 *
 * Two threads calling TransportSession::stop() concurrently on an active
 * session must result in exactly one call into the backend's stop().
 */
TEST(TransportSessionDirectTest, ConcurrentStopClaimsOwnershipExactlyOnce)
{
    FakeTransport transport;
    transport::detail::TransportSession session(transport);

    struct Listener : public ITransportListener {
        void on_message_received(MessagePtr, const Endpoint&) override
        {
        }
        void on_connection_lost(const Endpoint&) override
        {
        }
        void on_connection_established(const Endpoint&) override
        {
        }
        void on_error(Result) override
        {
        }
    } listener;

    ASSERT_EQ(session.start(listener), Result::SUCCESS);

    std::thread t1([&] { session.stop(); });
    std::thread t2([&] { session.stop(); });
    t1.join();
    t2.join();

    EXPECT_EQ(transport.stop_calls, 1u);
    EXPECT_LE(transport.max_concurrent_stops.load(), 1);
}

}  // namespace
