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
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "platform/memory.h"
#include "rpc/rpc_client.h"
#include "someip/message.h"
#include "static_pool_init.h"
#include "transport/transport.h"

/**
 * @test_case TC_RPC_SYNC_LIFETIME
 * @tests REQ_ARCH_002, REQ_ARCH_003, REQ_MSG_118
 *
 * Regression coverage for the RpcClient review follow-ups:
 *  - B2: RpcCallHandle 0 overloaded as both sentinel and legal key
 *        (PendingRegistration::armed_ + handle-skip in submit_call).
 *  - S3: shutdown() must not rethrow out of a void public API.
 *  - S4: the response deadline must not be charged for the blocking send.
 *  - S5: sessions must be released when a pending call is resolved.
 *  - (minor) redundant erase on the success path.
 */
namespace {

using namespace someip;
using transport::Endpoint;
using transport::ITransportListener;

constexpr uint16_t SERVICE = 0x2234;
constexpr uint16_t METHOD = 0x0142;
const Endpoint PEER("192.0.2.10", 32001);

// Minimal fake transport, modeled on tests/test_transport_injection.cpp's
// FakeTransport, trimmed to what RpcClient exercises. Adds a configurable
// send delay so tests can simulate a blocking transport send.
class FakeTransport final : public transport::ITransport {
   public:
    Result send_message(const Message& message, const Endpoint& endpoint) override
    {
        if (send_delay > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(send_delay);
        }
        if (!running_) {
            return Result::NOT_CONNECTED;
        }
        if (send_result != Result::SUCCESS) {
            return send_result;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            messages.push_back(message);
            destinations.push_back(endpoint);
        }
        if (on_send) {
            on_send(message);
        }
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
        return Endpoint("192.0.2.11", 32002);
    }

    void set_listener(ITransportListener* listener) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_ = listener;
    }

    // send_message() appends under mutex_, so a test thread polling for the
    // request to go out must read under it too -- an unguarded
    // `messages.empty()` spin is a genuine data race that ThreadSanitizer
    // reports against the vector's internal pointers.
    std::size_t sent_count()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages.size();
    }

    Result start() override
    {
        running_ = true;
        return start_result;
    }

    Result stop() override
    {
        running_ = remain_running_on_stop;
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

    bool emit(const MessagePtr& message, const Endpoint& sender = PEER)
    {
        ITransportListener* target = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_ || listener_ == nullptr) {
                return false;
            }
            target = listener_;
        }
        target->on_message_received(message, sender, get_local_endpoint());
        return true;
    }

    Result start_result{Result::SUCCESS};
    Result stop_result{Result::SUCCESS};
    Result send_result{Result::SUCCESS};
    bool remain_running_on_stop{false};
    std::chrono::milliseconds send_delay{0};
    std::vector<Message> messages;
    std::vector<Endpoint> destinations;
    std::function<void(const Message&)> on_send;

   private:
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    ITransportListener* listener_{nullptr};
};

/// Builds and emits a RESPONSE that matches `request` on the given transport.
bool reply_to(FakeTransport& transport, const Message& request, const platform::ByteBuffer& payload,
             ReturnCode return_code = ReturnCode::E_OK)
{
    auto response = platform::allocate_message();
    if (!response) {
        return false;
    }
    *response = Message(request.get_message_id(), request.get_request_id(), MessageType::RESPONSE,
                        return_code);
    response->set_payload(payload);
    return transport.emit(response);
}

/**
 * @test_case TC_RPC_SYNC_HANDLE_SENTINEL
 * @tests REQ_ARCH_002
 *
 * B2: submit_call's failure paths must fully unregister the pending-call
 * entry via the armed_-based RAII, exactly as they did for real (non-zero)
 * handles before the fix -- this exercises the refactored PendingRegistration
 * without relying on the sentinel/legal-value ambiguity.
 *
 * NOTE: the actual defect only manifests once next_call_handle_ wraps past
 * 2^32 (the (2^32)th submission produces handle 0). That path is not
 * reachable from a unit test in bounded time and RpcClientImpl exposes no
 * public seam to force the atomic counter's value, so the wrap itself is
 * verified by code inspection only (see PendingRegistration::armed_ and the
 * handle-skip logic in submit_call). This test instead pins down that the
 * cleanup mechanics those fixes rely on still behave correctly.
 */
TEST(RpcSyncLifetime, FailedSendLeavesNoDanglingRegistration)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());

    transport.send_result = Result::NETWORK_ERROR;
    unsigned failed_callbacks = 0;
    const auto failed_handle = client.call_method_async(
        SERVICE, METHOD, {}, [&](const rpc::RpcResponse&) { ++failed_callbacks; }, PEER);
    EXPECT_EQ(failed_handle, 0u);

    // A subsequent successful submission must not observe any trace of the
    // failed one (no leaked map entry, no leaked handle collision).
    transport.send_result = Result::SUCCESS;
    unsigned ok_callbacks = 0;
    const auto ok_handle = client.call_method_async(
        SERVICE, METHOD, {}, [&](const rpc::RpcResponse&) { ++ok_callbacks; }, PEER);
    ASSERT_NE(ok_handle, 0u);
    EXPECT_TRUE(client.cancel_call(ok_handle));
    EXPECT_EQ(ok_callbacks, 1u);

    // If the failed submission had leaked an entry, shutdown's sweep would
    // find and complete it a second time.
    client.shutdown();
    EXPECT_EQ(failed_callbacks, 0u);
    EXPECT_EQ(ok_callbacks, 1u);
}

#ifdef __cpp_exceptions
TEST(RpcSyncLifetime, ThrowingSendUnregistersTheAsyncSubmission)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    transport.on_send = [](const Message&) { throw std::runtime_error("send"); };
    unsigned callbacks = 0;
    EXPECT_THROW(client.call_method_async(
                     SERVICE, METHOD, {}, [&](const rpc::RpcResponse&) { ++callbacks; }, PEER),
                 std::runtime_error);
    transport.on_send = nullptr;
    // The entry armed just before the throwing send must already be gone;
    // otherwise this reply would find and complete a leaked registration.
    ASSERT_EQ(transport.messages.size(), 1u);
    EXPECT_TRUE(reply_to(transport, transport.messages[0], {0x01}));
    EXPECT_EQ(callbacks, 0u);
    EXPECT_NO_THROW(client.shutdown());
}
#endif

/**
 * @test_case TC_RPC_SHUTDOWN_NOTHROW
 * @tests REQ_ARCH_003
 *
 * S3: shutdown() is a void API that application RAII wrappers call from
 * their own destructors; a throwing pending-callback must not escape it,
 * and every pending callback must still be attempted.
 */
#ifdef __cpp_exceptions
TEST(RpcSyncLifetime, ShutdownSwallowsCallbackExceptionsAndCompletesAllCallbacks)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    unsigned invocations = 0;
    for (uint16_t method = METHOD; method < METHOD + 3; ++method) {
        ASSERT_NE(client.call_method_async(
                      SERVICE, method, {},
                      [&](const rpc::RpcResponse& response) {
                          ++invocations;
                          EXPECT_EQ(response.result, rpc::RpcResult::INTERNAL_ERROR);
                          throw std::runtime_error("boom");
                      },
                      PEER),
                  0u);
    }
    EXPECT_NO_THROW(client.shutdown());
    EXPECT_EQ(invocations, 3u);
    // The early "!running_" path must also stay throw-free on a repeat call.
    EXPECT_NO_THROW(client.shutdown());
}
#endif

TEST(RpcSyncLifetime, ShutdownCompletesPendingWaiterAndCallbackTogether)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());

    rpc::RpcTimeout timeout;
    timeout.response_timeout = std::chrono::seconds(5);
    auto sync_call = std::async(std::launch::async, [&] {
        return client.call_method_sync(SERVICE, METHOD, {}, PEER, timeout);
    });
    // Wait for the request to actually be sent before racing shutdown.
    while (transport.sent_count() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    unsigned async_invocations = 0;
    ASSERT_NE(client.call_method_async(
                  SERVICE, METHOD + 1, {},
                  [&](const rpc::RpcResponse& response) {
                      ++async_invocations;
                      EXPECT_EQ(response.result, rpc::RpcResult::INTERNAL_ERROR);
                  },
                  PEER),
              0u);

    client.shutdown();
    EXPECT_EQ(sync_call.get().result, rpc::RpcResult::INTERNAL_ERROR);
    EXPECT_EQ(async_invocations, 1u);
}

/**
 * @test_case TC_RPC_SYNC_DEADLINE_EXCLUDES_SEND
 * @tests REQ_ARCH_003, REQ_MSG_118
 *
 * S4: the response-timeout clock must start after the (possibly blocking)
 * send completes, not before it. Against the unfixed code, the deadline
 * captured before a slow send has already elapsed by the time submit_call
 * returns, so the call returns TIMEOUT immediately after the send with no
 * actual wait; the fixed code must still wait out the full response budget
 * afterwards.
 */
TEST(RpcSyncLifetime, ResponseTimeoutClockExcludesTheBlockingSend)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());

    constexpr auto send_delay = std::chrono::milliseconds(300);
    constexpr auto response_timeout = std::chrono::milliseconds(150);
    transport.send_delay = send_delay;

    rpc::RpcTimeout timeout;
    timeout.response_timeout = response_timeout;

    const auto wall_start = std::chrono::steady_clock::now();
    const auto result = client.call_method_sync(SERVICE, METHOD, {}, PEER, timeout);
    const auto wall_elapsed = std::chrono::steady_clock::now() - wall_start;

    EXPECT_EQ(result.result, rpc::RpcResult::TIMEOUT);
    // Fixed behavior: total wall time is (send + full response wait).
    // Unfixed behavior: the call returns right after the send completes,
    // i.e. wall time ~= send_delay, well short of this bound.
    EXPECT_GE(wall_elapsed, send_delay + response_timeout - std::chrono::milliseconds(20));
    // The reported elapsed time must reflect only the post-send wait.
    EXPECT_LT(result.response_time, send_delay);
}

/**
 * @test_case TC_RPC_SYNC_DEADLINE_ARBITRATION
 * @tests REQ_ARCH_003
 *
 * A response delivered while the deadline is already technically expired
 * must still win, and the arbitration must hold even though the clock now
 * starts later (post-send) than before the fix.
 */
TEST(RpcSyncLifetime, ResponseAlreadyDeliveredStillWinsOverExpiredDeadline)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    transport.on_send = [&](const Message& request) {
        EXPECT_TRUE(reply_to(transport, request, {0x77}));
    };

    rpc::RpcTimeout timeout;
    timeout.response_timeout = std::chrono::milliseconds(0);
    const auto result = client.call_method_sync(SERVICE, METHOD, {}, PEER, timeout);
    EXPECT_EQ(result.result, rpc::RpcResult::SUCCESS);
    ASSERT_EQ(result.return_values.size(), 1u);
    EXPECT_EQ(result.return_values[0], 0x77);
}

/**
 * @test_case TC_RPC_SUBMIT_FAILURE_REASON
 * @tests REQ_ARCH_003
 *
 * submit_call returning a null handle because the client is not running
 * must surface as SERVICE_NOT_AVAILABLE, not the generic INTERNAL_ERROR the
 * unfixed code reported for every submission failure.
 */
TEST(RpcSyncLifetime, SyncCallWhileNotRunningReportsServiceNotAvailable)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    // Deliberately not initialized.
    const auto result = client.call_method_sync(SERVICE, METHOD, {}, PEER);
    EXPECT_EQ(result.result, rpc::RpcResult::SERVICE_NOT_AVAILABLE);
    EXPECT_EQ(result.response_time, std::chrono::milliseconds(0));
}

TEST(RpcSyncLifetime, SyncCallAfterShutdownReportsServiceNotAvailable)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    client.shutdown();
    const auto result = client.call_method_sync(SERVICE, METHOD, {}, PEER);
    EXPECT_EQ(result.result, rpc::RpcResult::SERVICE_NOT_AVAILABLE);
}

/**
 * @test_case TC_RPC_SYNC_SUCCESS_NO_LEAK
 * @tests REQ_ARCH_002
 *
 * Minor fix: the success path must release its own registration so
 * ~PendingRegistration does not redundantly re-lock pending_calls_mutex_ to
 * erase an already-erased handle. Not independently observable through the
 * public API without instrumentation, so this is a baseline correctness /
 * no-hang check: many back-to-back successful synchronous calls must all
 * complete promptly and with correct payload correlation.
 */
TEST(RpcSyncLifetime, ManySequentialSyncCallsCompletePromptly)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    transport.on_send = [&](const Message& request) {
        const auto payload = request.get_payload();
        EXPECT_TRUE(reply_to(transport, request, payload));
    };

    rpc::RpcTimeout timeout;
    timeout.response_timeout = std::chrono::seconds(2);
    for (uint8_t i = 0; i < 50; ++i) {
        const auto result = client.call_method_sync(SERVICE, METHOD, {i}, PEER, timeout);
        ASSERT_EQ(result.result, rpc::RpcResult::SUCCESS);
        ASSERT_EQ(result.return_values.size(), 1u);
        EXPECT_EQ(result.return_values[0], i);
    }
}

/**
 * @test_case TC_RPC_CANCEL_STILL_WORKS
 * @tests REQ_ARCH_002
 *
 * Sanity check that threading a session release through cancel_call's erase
 * did not disturb its existing contract.
 */
TEST(RpcSyncLifetime, CancelCallStillCompletesCallbackAndReportsUnknownHandle)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    unsigned invocations = 0;
    const auto handle = client.call_method_async(
        SERVICE, METHOD, {},
        [&](const rpc::RpcResponse& response) {
            ++invocations;
            EXPECT_EQ(response.result, rpc::RpcResult::INTERNAL_ERROR);
        },
        PEER);
    ASSERT_NE(handle, 0u);
    EXPECT_TRUE(client.cancel_call(handle));
    EXPECT_EQ(invocations, 1u);
    EXPECT_FALSE(client.cancel_call(handle));
}

}  // namespace
