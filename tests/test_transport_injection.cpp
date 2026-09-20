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
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <stdexcept>

#include "../src/common/callback_storage.h"
#include "events/event_publisher.h"
#include "events/event_subscriber.h"
#include "platform/memory.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_server.h"
#include "someip/message.h"
#include "static_pool_init.h"
#include "transport/transport.h"

/**
 * @test_case TC_TRANSPORT_INJECTION
 * @tests REQ_ARCH_002, REQ_ARCH_003, REQ_ARCH_004
 */
namespace {

using namespace someip;
using transport::Endpoint;
using transport::ITransportListener;

constexpr uint16_t SERVICE = 0x1234;
constexpr uint16_t METHOD = 0x0042;
constexpr uint16_t EVENT = 0x8001;
constexpr uint16_t GROUP = 0x0001;
const Endpoint PEER("192.0.2.1", 31001);

class FakeTransport final : public transport::ITransport {
   public:
    Result send_message(const Message& message, const Endpoint& endpoint) override
    {
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
            sent_.notify_all();
        }
        if (on_send) {
            on_send(message);
        }
        return Result::SUCCESS;
    }

    MessagePtr receive_message() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queued_messages.empty()) {
            return nullptr;
        }
        auto message = queued_messages.front();
        queued_messages.pop_front();
        return message;
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
        return Endpoint("192.0.2.2", 31002);
    }

    void set_listener(ITransportListener* listener) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_ = listener;
    }

    Result start() override
    {
        ++starts;
        running_ = true;  // Failed starts may still need cleanup.
        return start_result;
    }

    Result stop() override
    {
        ++stops;
        listener_present_at_stop = listener() != nullptr;
        running_ = remain_running_on_stop;
        if (on_stop) {
            on_stop();
        }
        std::unique_lock<std::mutex> lock(mutex_);
        drained_.wait(lock, [this] { return in_flight_ == 0; });
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
            ++in_flight_;
        }
        struct CompleteDispatch {
            explicit CompleteDispatch(FakeTransport& transport) : transport(transport)
            {
            }
            ~CompleteDispatch()
            {
                {
                    std::lock_guard<std::mutex> lock(transport.mutex_);
                    --transport.in_flight_;
                }
                transport.drained_.notify_all();
            }
            FakeTransport& transport;
        } completion(*this);
        if (before_dispatch) {
            before_dispatch();
        }
        target->on_message_received(message, sender, get_local_endpoint());
        return true;
    }

    void wait_for_send()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        sent_.wait(lock, [this] { return !messages.empty(); });
    }

    Result start_result{Result::SUCCESS};
    Result stop_result{Result::SUCCESS};
    Result send_result{Result::SUCCESS};
    bool remain_running_on_stop{false};
    bool listener_present_at_stop{false};
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> stops{0};
    std::vector<Message> messages;
    std::vector<Endpoint> destinations;
    std::deque<MessagePtr> queued_messages;
    std::function<void()> before_dispatch;
    std::function<void()> on_stop;
    std::function<void(const Message&)> on_send;

   private:
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable drained_;
    std::condition_variable sent_;
    ITransportListener* listener_{nullptr};
    unsigned in_flight_{0};
};

template <typename Facade>
std::unique_ptr<Facade> make_facade(FakeTransport& transport)
{
    if constexpr (std::is_same_v<Facade, events::EventPublisher>) {
        return std::make_unique<Facade>(SERVICE, 1, transport);
    }
    else {
        return std::make_unique<Facade>(SERVICE, transport);
    }
}

/// Builds a message the facade's receive handler accepts, so dispatch reaches
/// the facade mutex instead of returning early on a message-type check.
/// EventPublisher has no receive handler body, so nothing reaches a lock there.
template <typename Facade>
MessagePtr make_dispatch_message()
{
    MessagePtr message = platform::allocate_message();
    if (!message) {
        return message;
    }
    if constexpr (std::is_same_v<Facade, rpc::RpcClient>) {
        *message = Message(MessageId(SERVICE, METHOD), RequestId(7, 1), MessageType::RESPONSE,
                           ReturnCode::E_OK);
    }
    else if constexpr (std::is_same_v<Facade, events::EventSubscriber>) {
        *message = Message(MessageId(SERVICE, EVENT), RequestId(0, 1), MessageType::NOTIFICATION,
                           ReturnCode::E_OK);
    }
    else {
        *message = Message(MessageId(SERVICE, METHOD), RequestId(7, 1), MessageType::REQUEST,
                           ReturnCode::E_OK);
    }
    return message;
}

template <typename Facade>
class TransportInjectionTest : public ::testing::Test {};

using Facades = ::testing::Types<rpc::RpcClient, rpc::RpcServer, events::EventPublisher,
                                 events::EventSubscriber>;
struct FacadeNames {
    template <typename Facade>
    static std::string GetName(int)
    {
        if constexpr (std::is_same_v<Facade, rpc::RpcClient>) {
            return "RpcClient";
        }
        else if constexpr (std::is_same_v<Facade, rpc::RpcServer>) {
            return "RpcServer";
        }
        else if constexpr (std::is_same_v<Facade, events::EventPublisher>) {
            return "EventPublisher";
        }
        else {
            return "EventSubscriber";
        }
    }
};
TYPED_TEST_SUITE(TransportInjectionTest, Facades, FacadeNames);

TYPED_TEST(TransportInjectionTest, ConstructionDoesNotTouchBorrowedTransport)
{
    FakeTransport transport;
    {
        auto facade = make_facade<TypeParam>(transport);
        EXPECT_EQ(transport.listener(), nullptr);
        EXPECT_EQ(facade->get_transport_result(), Result::SUCCESS);
    }
    EXPECT_EQ(transport.starts, 0u);
    EXPECT_EQ(transport.stops, 0u);
    EXPECT_EQ(transport.listener(), nullptr);
}

TYPED_TEST(TransportInjectionTest, ReattachesOnRestartAndStopsExactlyOncePerStart)
{
    FakeTransport transport;
    {
        auto facade = make_facade<TypeParam>(transport);
        ASSERT_TRUE(facade->initialize());
        EXPECT_TRUE(facade->initialize());
        EXPECT_EQ(transport.starts, 1u);
        EXPECT_NE(transport.listener(), nullptr);
        facade->shutdown();
        EXPECT_EQ(transport.stops, 1u);
        EXPECT_EQ(transport.listener(), nullptr);
        facade->shutdown();
        EXPECT_EQ(transport.stops, 1u);
        ASSERT_TRUE(facade->initialize());
        EXPECT_EQ(transport.starts, 2u);
        EXPECT_NE(transport.listener(), nullptr);
    }
    EXPECT_EQ(transport.stops, 2u);
    EXPECT_EQ(transport.listener(), nullptr);
    EXPECT_FALSE(transport.is_running());
}

TYPED_TEST(TransportInjectionTest, FailedStartDetachesAndCleansUpBeforeRetry)
{
    FakeTransport transport;
    transport.start_result = Result::NETWORK_ERROR;
    {
        auto facade = make_facade<TypeParam>(transport);
        EXPECT_FALSE(facade->initialize());
        EXPECT_EQ(facade->get_transport_result(), Result::NETWORK_ERROR);
        EXPECT_FALSE(transport.is_running());
        EXPECT_EQ(transport.listener(), nullptr);
        EXPECT_EQ(transport.stops, 1u);
        transport.start_result = Result::SUCCESS;
        ASSERT_TRUE(facade->initialize());
        EXPECT_EQ(facade->get_transport_result(), Result::SUCCESS);
    }
    EXPECT_EQ(transport.stops, 2u);
    EXPECT_EQ(transport.listener(), nullptr);
}

TYPED_TEST(TransportInjectionTest, RejectsActiveTransportWithoutStealingListenerOrStoppingIt)
{
    FakeTransport transport;
    auto owner = make_facade<TypeParam>(transport);
    ASSERT_TRUE(owner->initialize());
    auto* listener = transport.listener();
    {
        auto rejected = make_facade<TypeParam>(transport);
        EXPECT_EQ(transport.listener(), listener);
        EXPECT_FALSE(rejected->initialize());
        EXPECT_EQ(rejected->get_transport_result(), Result::INVALID_STATE);
    }
    EXPECT_EQ(transport.listener(), listener);
    EXPECT_TRUE(transport.is_running());
    EXPECT_EQ(transport.starts, 1u);
    EXPECT_EQ(transport.stops, 0u);
}

TYPED_TEST(TransportInjectionTest, ReportsStopAndFailedStartCleanupErrors)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    transport.start_result = Result::NETWORK_ERROR;
    transport.stop_result = Result::INTERNAL_ERROR;
    EXPECT_FALSE(facade->initialize());
    EXPECT_EQ(facade->get_transport_result(), Result::INTERNAL_ERROR);
    EXPECT_EQ(transport.listener(), nullptr);

    transport.start_result = Result::SUCCESS;
    ASSERT_TRUE(facade->initialize());
    facade->shutdown();
    EXPECT_EQ(facade->get_transport_result(), Result::INTERNAL_ERROR);
    EXPECT_EQ(transport.listener(), nullptr);
    EXPECT_FALSE(transport.is_running());
}

/**
 * @test_case TC_INJECTED_RECEIVE_SESSION_DRAIN
 * @tests REQ_ARCH_002, REQ_ARCH_003
 */
TYPED_TEST(TransportInjectionTest, StopsBeforeDetachingAndReleasesQueuedMessagesOnEveryRestart)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    for (unsigned iteration = 0; iteration < 40; ++iteration) {
        ASSERT_TRUE(facade->initialize());
        auto message = platform::allocate_message();
        ASSERT_NE(message, nullptr);
        transport.queued_messages.push_back(message);
        message.reset();
        facade->shutdown();
        EXPECT_TRUE(transport.listener_present_at_stop);
        EXPECT_TRUE(transport.queued_messages.empty());
        EXPECT_EQ(transport.listener(), nullptr);
    }
}

/**
 * @test_case TC_INJECTED_STOP_RETRY
 * @tests REQ_ARCH_004
 */
TYPED_TEST(TransportInjectionTest, RetriesStopWithoutStealingAnotherSession)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    ASSERT_TRUE(facade->initialize());
    transport.stop_result = Result::NETWORK_ERROR;
    transport.remain_running_on_stop = true;
    facade->shutdown();
    EXPECT_EQ(facade->get_transport_result(), Result::NETWORK_ERROR);
    EXPECT_EQ(transport.stops, 1u);
    EXPECT_EQ(transport.listener(), nullptr);
    EXPECT_FALSE(facade->initialize());

    transport.stop_result = Result::SUCCESS;
    transport.remain_running_on_stop = false;
    facade->shutdown();
    EXPECT_EQ(transport.stops, 2u);
    EXPECT_EQ(facade->get_transport_result(), Result::SUCCESS);
    ASSERT_TRUE(facade->initialize());
}

/**
 * @test_case TC_INJECTED_FAILED_START_RETRY
 * @tests REQ_ARCH_003, REQ_ARCH_004
 */
TYPED_TEST(TransportInjectionTest, FailedStartCleanupCanBeRetried)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    transport.start_result = Result::NETWORK_ERROR;
    transport.stop_result = Result::INTERNAL_ERROR;
    transport.remain_running_on_stop = true;
    EXPECT_FALSE(facade->initialize());
    EXPECT_EQ(facade->get_transport_result(), Result::INTERNAL_ERROR);
    EXPECT_EQ(transport.listener(), nullptr);
    transport.remain_running_on_stop = false;
    transport.stop_result = Result::SUCCESS;
    facade->shutdown();
    EXPECT_EQ(transport.stops, 2u);
    transport.start_result = Result::SUCCESS;
    ASSERT_TRUE(facade->initialize());
}

TYPED_TEST(TransportInjectionTest, SuccessfulStopMustLeaveTheBackendStopped)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    ASSERT_TRUE(facade->initialize());
    transport.remain_running_on_stop = true;
    facade->shutdown();
    EXPECT_EQ(facade->get_transport_result(), Result::INVALID_STATE);
    transport.remain_running_on_stop = false;
    facade->shutdown();
    EXPECT_EQ(facade->get_transport_result(), Result::SUCCESS);
}

TYPED_TEST(TransportInjectionTest, CanReplaceFacadeWithoutReplacingTransport)
{
    FakeTransport transport;
    {
        auto facade = make_facade<TypeParam>(transport);
        ASSERT_TRUE(facade->initialize());
    }
    {
        auto replacement = make_facade<TypeParam>(transport);
        ASSERT_TRUE(replacement->initialize());
        EXPECT_NE(transport.listener(), nullptr);
    }
    EXPECT_EQ(transport.starts, 2u);
    EXPECT_EQ(transport.stops, 2u);
    EXPECT_EQ(transport.listener(), nullptr);
}

TYPED_TEST(TransportInjectionTest, ShutdownWaitsForInFlightDispatchWithoutHoldingFacadeLocks)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    ASSERT_TRUE(facade->initialize());
    // The handler must accept this message, otherwise it returns before taking
    // the facade mutex and the test cannot detect stop-after-lock ordering.
    MessagePtr message = make_dispatch_message<TypeParam>();
    ASSERT_NE(message, nullptr);
    std::promise<void> entered;
    std::promise<void> release;
    std::promise<void> stopping;
    auto released = release.get_future().share();
    transport.before_dispatch = [&] {
        entered.set_value();
        released.wait();
    };
    transport.on_stop = [&] { stopping.set_value(); };
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    entered.get_future().wait();
    auto shutdown = std::async(std::launch::async, [&] { facade->shutdown(); });
    stopping.get_future().wait();
    EXPECT_EQ(shutdown.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
    release.set_value();
    EXPECT_TRUE(dispatch.get());
    shutdown.get();
    EXPECT_EQ(transport.listener(), nullptr);
    EXPECT_FALSE(transport.emit(message));
}

TEST(TransportInjectionRouting, RpcClientUsesInjectedTransportAndCorrelatesResponse)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    client.set_remote_endpoint(PEER);
    bool replied = false;
    const auto handle =
        client.call_method_async(SERVICE, METHOD, {0x11}, [&](const rpc::RpcResponse& response) {
            replied = true;
            EXPECT_EQ(response.result, rpc::RpcResult::SUCCESS);
            ASSERT_EQ(response.return_values.size(), 1u);
            EXPECT_EQ(response.return_values[0], 0x22);
        });
    ASSERT_NE(handle, 0u);
    ASSERT_EQ(transport.messages.size(), 1u);
    EXPECT_EQ(transport.destinations[0], PEER);
    EXPECT_EQ(client.get_local_endpoint(), transport.get_local_endpoint());
    const auto& request = transport.messages[0];
    auto response = platform::allocate_message();
    ASSERT_NE(response, nullptr);
    *response = Message(request.get_message_id(), request.get_request_id(), MessageType::RESPONSE,
                        ReturnCode::E_OK);
    response->set_payload({0x22});
    ASSERT_TRUE(transport.emit(response));
    EXPECT_TRUE(replied);
    transport.send_result = Result::NETWORK_ERROR;
    EXPECT_FALSE(client.send_request_no_return(SERVICE, METHOD, {}, PEER));
}

TEST(TransportInjectionRouting, RpcServerRepliesToInjectedSender)
{
    FakeTransport transport;
    rpc::RpcServer server(SERVICE, transport);
    ASSERT_TRUE(server.register_method(
        METHOD,
        [](uint16_t, uint16_t, const platform::ByteBuffer& input, platform::ByteBuffer& output) {
            output = input;
            return rpc::RpcResult::SUCCESS;
        }));
    ASSERT_TRUE(server.initialize());
    auto request = platform::allocate_message();
    ASSERT_NE(request, nullptr);
    *request = Message(MessageId(SERVICE, METHOD), RequestId(7, 3), MessageType::REQUEST,
                       ReturnCode::E_OK);
    request->set_payload({0x33});
    ASSERT_TRUE(transport.emit(request));
    ASSERT_EQ(transport.messages.size(), 1u);
    EXPECT_EQ(transport.messages[0].get_message_type(), MessageType::RESPONSE);
    EXPECT_EQ(transport.messages[0].get_request_id(), request->get_request_id());
    EXPECT_EQ(transport.messages[0].get_payload(), request->get_payload());
    EXPECT_EQ(transport.destinations[0], PEER);
    EXPECT_EQ(server.get_local_endpoint(), transport.get_local_endpoint());
}

TEST(TransportInjectionRouting, EventPublisherUsesInjectedTransport)
{
    FakeTransport transport;
    events::EventPublisher publisher(SERVICE, 1, transport);
    events::EventConfig config;
    config.event_id = EVENT;
    config.eventgroup_id = GROUP;
    config.notification_type = events::NotificationType::ON_CHANGE;
    ASSERT_TRUE(publisher.register_event(config));
    ASSERT_TRUE(publisher.initialize());
    publisher.set_default_client_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(publisher.handle_subscription(GROUP, 7));
    ASSERT_TRUE(publisher.publish_event(EVENT, {0x44}));
    ASSERT_EQ(transport.messages.size(), 1u);
    EXPECT_EQ(transport.messages[0].get_message_type(), MessageType::NOTIFICATION);
    EXPECT_EQ(transport.messages[0].get_method_id(), EVENT);
    ASSERT_EQ(transport.messages[0].get_payload().size(), 1u);
    EXPECT_EQ(transport.messages[0].get_payload()[0], 0x44);
    EXPECT_EQ(transport.destinations[0], PEER);
}

TEST(TransportInjectionRouting, EventSubscriberReceivesInjectedNotification)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    unsigned notifications = 0;
    ASSERT_TRUE(subscriber.subscribe_eventgroup(SERVICE, 1, GROUP,
                                                [&](const events::EventNotification& notification) {
                                                    ++notifications;
                                                    EXPECT_EQ(notification.event_id, EVENT);
                                                    ASSERT_EQ(notification.event_data.size(), 1u);
                                                    EXPECT_EQ(notification.event_data[0], 0x55);
                                                }));
    ASSERT_EQ(transport.destinations.size(), 1u);
    EXPECT_EQ(transport.destinations[0], PEER);
    auto message = platform::allocate_message();
    ASSERT_NE(message, nullptr);
    *message = Message(MessageId(SERVICE, EVENT), RequestId(0, 1), MessageType::NOTIFICATION,
                       ReturnCode::E_OK);
    message->set_payload({0x55});
    ASSERT_TRUE(transport.emit(message));
    EXPECT_EQ(notifications, 1u);

    bool stale_field_callback = false;
    ASSERT_TRUE(subscriber.request_field(
        SERVICE, 1, EVENT, [&](const events::EventNotification&) { stale_field_callback = true; }));
    subscriber.shutdown();
    ASSERT_TRUE(subscriber.initialize());
    ASSERT_TRUE(transport.emit(message));
    EXPECT_FALSE(stale_field_callback);
    EXPECT_EQ(notifications, 1u);
}

TEST(TransportInjectionRouting, SubscriberReleasesCallbackCapturesOutsideLocks)
{
    // Destroying this while subscriptions_mutex_ is held would deadlock, so it
    // detects a capture released under the lock rather than after it.
    struct ReleaseProbe {
        ReleaseProbe(events::EventSubscriber& subscriber, unsigned& releases)
            : subscriber(subscriber), releases(releases)
        {
        }
        ~ReleaseProbe()
        {
            if (subscriber.get_active_subscriptions().empty()) {
                ++releases;
            }
        }
        events::EventSubscriber& subscriber;
        unsigned& releases;
    };

    FakeTransport transport;
    unsigned subscription_releases = 0;
    unsigned field_releases = 0;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());

    // Independent probes count both releases; the helper test checks its actual mutex.
    auto subscription_probe = std::make_shared<ReleaseProbe>(subscriber, subscription_releases);
    auto field_probe = std::make_shared<ReleaseProbe>(subscriber, field_releases);
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE, 1, GROUP, [subscription_probe](const events::EventNotification&) {}));
    ASSERT_TRUE(subscriber.request_field(SERVICE, 1, EVENT,
                                         [field_probe](const events::EventNotification&) {}));
    subscription_probe.reset();
    field_probe.reset();

    subscriber.shutdown();
    EXPECT_EQ(subscription_releases, 1u);
    EXPECT_EQ(field_releases, 1u);
}

/**
 * @test_case TC_EVENT_NOTIFICATION_REENTRANCY
 * @tests REQ_ARCH_002
 */
TEST(TransportInjectionRouting, NotificationCallbackCanQueryAndUnsubscribe)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    unsigned calls = 0;
    ASSERT_TRUE(
        subscriber.subscribe_eventgroup(SERVICE, 1, GROUP, [&](const events::EventNotification&) {
            ++calls;
            EXPECT_EQ(subscriber.get_active_subscriptions().size(), 1u);
            EXPECT_TRUE(subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP));
        }));
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    ASSERT_TRUE(transport.emit(message));
    ASSERT_TRUE(transport.emit(message));
    EXPECT_EQ(calls, 1u);
    EXPECT_TRUE(subscriber.get_active_subscriptions().empty());
}

/**
 * @test_case TC_EVENT_FIELD_REENTRANCY
 * @tests REQ_ARCH_002
 */
TEST(TransportInjectionRouting, FieldCallbackCanRequestTheNextValue)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    unsigned calls = 0;
    ASSERT_TRUE(subscriber.request_field(SERVICE, 1, EVENT, [&](const events::EventNotification&) {
        ++calls;
        EXPECT_TRUE(subscriber.get_active_subscriptions().empty());
        EXPECT_TRUE(subscriber.request_field(SERVICE, 1, EVENT,
                                             [&](const events::EventNotification&) { ++calls; }));
    }));
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    ASSERT_TRUE(transport.emit(message));
    EXPECT_EQ(calls, 1u);
    ASSERT_TRUE(transport.emit(message));
    EXPECT_EQ(calls, 2u);
}

/**
 * @test_case TC_EVENT_FIELD_SEND_FAILURE
 * @tests REQ_ARCH_003, REQ_ARCH_004
 */
TEST(TransportInjectionRouting, FailedFieldSendDoesNotRetainCallback)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    transport.send_result = Result::NETWORK_ERROR;
    unsigned calls = 0;
    EXPECT_FALSE(subscriber.request_field(SERVICE, 1, EVENT,
                                          [&](const events::EventNotification&) { ++calls; }));
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    ASSERT_TRUE(transport.emit(message));
    EXPECT_EQ(calls, 0u);
}

/**
 * @test_case TC_RPC_HANDLER_RELEASE
 * @tests REQ_ARCH_002, REQ_ARCH_003
 */
TEST(TransportInjectionRouting, ServerReleasesLastHandlerReferenceOutsideMethodMutex)
{
    struct Capture {
        Capture(rpc::RpcServer& server, unsigned& releases) : server(server), releases(releases)
        {
        }
        ~Capture()
        {
            if (server.get_registered_methods().empty()) {
                ++releases;
            }
        }
        rpc::RpcServer& server;
        unsigned& releases;
    };
    FakeTransport transport;
    unsigned releases = 0;
    rpc::RpcServer server(SERVICE, transport);
    auto capture = std::make_shared<Capture>(server, releases);
    ASSERT_TRUE(server.register_method(
        METHOD, [capture](uint16_t, uint16_t, const platform::ByteBuffer&, platform::ByteBuffer&) {
            return rpc::RpcResult::SUCCESS;
        }));
    capture.reset();
    ASSERT_TRUE(server.initialize());
    server.shutdown();
    EXPECT_EQ(releases, 1u);
}

/**
 * @test_case TC_RPC_SYNC_TIMEOUT_DURING_STOP
 * @tests REQ_ARCH_003, REQ_MSG_118
 */
TEST(TransportInjectionRouting, SyncTimeoutCancelsWhileTransportStopIsBlocked)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    std::promise<void> stopping;
    std::promise<void> release;
    auto released = release.get_future().share();
    transport.on_stop = [&] {
        stopping.set_value();
        released.wait();
    };
    rpc::RpcTimeout timeout;
    timeout.response_timeout = std::chrono::milliseconds(100);
    auto call = std::async(std::launch::async, [&] {
        return client.call_method_sync(SERVICE, METHOD, {}, PEER, timeout);
    });
    transport.wait_for_send();
    auto shutdown = std::async(std::launch::async, [&] { client.shutdown(); });
    stopping.get_future().wait();
    const auto status = call.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);
    if (status == std::future_status::ready) {
        EXPECT_EQ(call.get().result, rpc::RpcResult::TIMEOUT);
    }
    release.set_value();
    shutdown.get();
    if (call.valid()) {
        static_cast<void>(call.get());
    }
}

/**
 * @test_case TC_RPC_SYNC_EXTRACTED_CALLBACK_LIFETIME
 * @tests REQ_ARCH_003, REQ_MSG_118
 */
TEST(TransportInjectionRouting, SyncWaiterCompletesWhileUnrelatedShutdownCallbackBlocks)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    rpc::RpcTimeout timeout;
    timeout.response_timeout = std::chrono::seconds(5);
    auto sync_call = std::async(std::launch::async, [&] {
        return client.call_method_sync(SERVICE, METHOD, {}, PEER, timeout);
    });
    transport.wait_for_send();

    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    const auto blocker = client.call_method_async(
        SERVICE, METHOD + 1, {},
        [&](const rpc::RpcResponse&) {
            entered.set_value();
            released.wait();
        },
        PEER);
    EXPECT_NE(blocker, 0u);
    if (blocker == 0) {
        static_cast<void>(sync_call.get());
        return;
    }
    auto shutdown = std::async(std::launch::async, [&] { client.shutdown(); });
    entered.get_future().wait();
    // Synchronous completion must not wait for an unrelated application callback.
    const auto status = sync_call.wait_for(std::chrono::seconds(1));
    EXPECT_EQ(status, std::future_status::ready);
    if (status == std::future_status::ready) {
        // Map iteration may have already delivered the sync shutdown completion.
        EXPECT_EQ(sync_call.get().result, rpc::RpcResult::INTERNAL_ERROR);
    }
    release.set_value();
    shutdown.get();
    if (sync_call.valid()) {
        const auto result = sync_call.get().result;
        EXPECT_EQ(result, rpc::RpcResult::INTERNAL_ERROR);
    }
}

TEST(TransportInjectionRouting, SyncResponseAlreadyDeliveredWinsOverExpiredDeadline)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    transport.on_send = [&](const Message& request) {
        auto response = platform::allocate_message();
        ASSERT_NE(response, nullptr);
        *response = Message(request.get_message_id(), request.get_request_id(),
                            MessageType::RESPONSE, ReturnCode::E_OK);
        response->set_payload({0xAB});
        EXPECT_TRUE(transport.emit(response));
    };
    rpc::RpcTimeout timeout;
    timeout.response_timeout = std::chrono::milliseconds(0);
    const auto result = client.call_method_sync(SERVICE, METHOD, {}, PEER, timeout);
    EXPECT_EQ(result.result, rpc::RpcResult::SUCCESS);
    ASSERT_EQ(result.return_values.size(), 1u);
    EXPECT_EQ(result.return_values[0], 0xAB);
}

#ifdef __cpp_exceptions
TEST(TransportInjectionRouting, ThrowingSendUnregistersTheSynchronousWaiter)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    transport.on_send = [](const Message&) { throw std::runtime_error("send"); };
    EXPECT_THROW(client.call_method_sync(SERVICE, METHOD, {}, PEER), std::runtime_error);
    // This is the first call on a fresh client; its registration must already be gone.
    EXPECT_FALSE(client.cancel_call(1));
    ASSERT_EQ(transport.messages.size(), 1u);
    auto response = platform::allocate_message();
    ASSERT_NE(response, nullptr);
    *response =
        Message(transport.messages[0].get_message_id(), transport.messages[0].get_request_id(),
                MessageType::RESPONSE, ReturnCode::E_OK);
    EXPECT_TRUE(transport.emit(response));
    EXPECT_NO_THROW(client.shutdown());
}

TEST(TransportInjectionRouting, ShutdownCompletesAllCallsBeforeRethrowingCallbackFailure)
{
    FakeTransport transport;
    rpc::RpcClient client(7, transport);
    ASSERT_TRUE(client.initialize());
    unsigned callbacks = 0;
    for (uint16_t method = METHOD; method < METHOD + 3; ++method) {
        ASSERT_NE(client.call_method_async(
                      SERVICE, method, {},
                      [&](const rpc::RpcResponse& response) {
                          ++callbacks;
                          EXPECT_EQ(response.result, rpc::RpcResult::INTERNAL_ERROR);
                          throw std::runtime_error("callback");
                      },
                      PEER),
                  0u);
    }
    EXPECT_THROW(client.shutdown(), std::runtime_error);
    EXPECT_EQ(callbacks, 3u);
    EXPECT_FALSE(transport.is_running());
    EXPECT_NO_THROW(client.shutdown());
}
#endif

TYPED_TEST(TransportInjectionTest, FailedStartPreservesTheLendersQueuedMessages)
{
    FakeTransport transport;
    auto message = platform::allocate_message();
    ASSERT_NE(message, nullptr);
    transport.queued_messages.push_back(message);
    transport.start_result = Result::NETWORK_ERROR;
    auto facade = make_facade<TypeParam>(transport);
    EXPECT_FALSE(facade->initialize());
    EXPECT_EQ(facade->get_transport_result(), Result::NETWORK_ERROR);
    ASSERT_EQ(transport.queued_messages.size(), 1u);
    EXPECT_EQ(transport.receive_message(), message);
}

TYPED_TEST(TransportInjectionTest, StartErrorRemainsTheCauseWhenCleanupReturnsSuccess)
{
    FakeTransport transport;
    auto facade = make_facade<TypeParam>(transport);
    transport.start_result = Result::NETWORK_ERROR;
    transport.remain_running_on_stop = true;
    EXPECT_FALSE(facade->initialize());
    EXPECT_EQ(facade->get_transport_result(), Result::NETWORK_ERROR);
    transport.remain_running_on_stop = false;
    facade->shutdown();
    EXPECT_EQ(facade->get_transport_result(), Result::SUCCESS);
}

TEST(TransportInjectionRouting, DuplicateFieldRequestPreservesTheAcceptedCallback)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    unsigned first = 0;
    unsigned second = 0;
    ASSERT_TRUE(subscriber.request_field(SERVICE, 1, EVENT,
                                         [&](const events::EventNotification&) { ++first; }));
    transport.send_result = Result::NETWORK_ERROR;
    EXPECT_FALSE(subscriber.request_field(SERVICE, 1, EVENT,
                                          [&](const events::EventNotification&) { ++second; }));
    transport.send_result = Result::SUCCESS;
    EXPECT_FALSE(subscriber.request_field(SERVICE, 1, EVENT,
                                          [&](const events::EventNotification&) { ++second; }));
    ASSERT_EQ(transport.messages.size(), 1u);
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    EXPECT_TRUE(transport.emit(message));
    EXPECT_EQ(first, 1u);
    EXPECT_EQ(second, 0u);
    EXPECT_TRUE(subscriber.request_field(SERVICE, 1, EVENT,
                                         [&](const events::EventNotification&) { ++second; }));
}

TEST(TransportInjectionRouting, CallbackStorageDestroysEachExtractedValueOutsideItsMutex)
{
    struct Probe {
        Probe(platform::Mutex& mutex, bool& unlocked) : mutex(mutex), unlocked(unlocked)
        {
        }
        ~Probe()
        {
            unlocked = mutex.try_lock();
            if (unlocked) {
                mutex.unlock();
            }
        }
        platform::Mutex& mutex;
        bool& unlocked;
    };
    platform::Mutex mutex;
    bool unlocked = false;
    platform::UnorderedMap<unsigned, std::shared_ptr<Probe>> entries;
    entries[1] = std::make_shared<Probe>(mutex, unlocked);
    someip::detail::release_entries(entries, mutex);
    EXPECT_TRUE(entries.empty());
    EXPECT_TRUE(unlocked);
}

TEST(TransportInjectionRouting, CallbackStorageDrainUsesAnInitialEntryBound)
{
    struct Reinsert {
        explicit Reinsert(platform::UnorderedMap<unsigned, std::shared_ptr<Reinsert>>& entries)
            : entries(entries) {}
        ~Reinsert() { entries[2] = nullptr; }
        platform::UnorderedMap<unsigned, std::shared_ptr<Reinsert>>& entries;
    };
    platform::UnorderedMap<unsigned, std::shared_ptr<Reinsert>> entries;
    platform::Mutex mutex;
    entries[1] = std::make_shared<Reinsert>(entries);
    someip::detail::release_entries(entries, mutex);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_NE(entries.find(2), entries.end());
}

TEST(TransportInjectionRouting, ServerRejectsRegistrationDuringShutdownButAllowsItBeforeRestart)
{
    FakeTransport transport;
    rpc::RpcServer server(SERVICE, transport);
    auto handler = [](uint16_t, uint16_t, const platform::ByteBuffer&, platform::ByteBuffer&) {
        return rpc::RpcResult::SUCCESS;
    };
    ASSERT_TRUE(server.register_method(METHOD, handler));
    ASSERT_TRUE(server.initialize());
    transport.on_stop = [&] { EXPECT_FALSE(server.register_method(METHOD + 1, handler)); };
    server.shutdown();
    EXPECT_TRUE(server.get_registered_methods().empty());
    EXPECT_TRUE(server.register_method(METHOD + 1, handler));
    transport.on_stop = nullptr;
}

bool wait_for_subscription_count(events::EventSubscriber& subscriber, size_t expected)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        if (subscriber.get_active_subscriptions().size() == expected) {
            return true;
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

TEST(TransportInjectionRouting, ExternalUnsubscribeWaitsForCallbackAndCaptureRelease)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    ASSERT_TRUE(
        subscriber.subscribe_eventgroup(SERVICE, 1, GROUP, [&](const events::EventNotification&) {
            entered.set_value();
            released.wait();
        }));
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    entered.get_future().wait();
    auto unsubscribe = std::async(
        std::launch::async, [&] { return subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP); });
    EXPECT_TRUE(wait_for_subscription_count(subscriber, 0));
    EXPECT_EQ(unsubscribe.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
    release.set_value();
    EXPECT_TRUE(dispatch.get());
    EXPECT_TRUE(unsubscribe.get());
}

TEST(TransportInjectionRouting, LaterNotificationsDoNotExtendAnUnsubscribeBarrier)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    std::promise<void> first_entered;
    std::promise<void> first_release;
    auto first_released = first_release.get_future().share();
    std::promise<void> later_entered;
    std::promise<void> later_release;
    auto later_released = later_release.get_future().share();
    ASSERT_TRUE(
        subscriber.subscribe_eventgroup(SERVICE, 1, GROUP, [&](const events::EventNotification&) {
            first_entered.set_value();
            first_released.wait();
        }));
    ASSERT_TRUE(subscriber.subscribe_eventgroup(SERVICE + 1, 1, GROUP,
                                                [&](const events::EventNotification&) {
                                                    later_entered.set_value();
                                                    later_released.wait();
                                                }));
    auto first_message = make_dispatch_message<events::EventSubscriber>();
    auto later_message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(first_message, nullptr);
    ASSERT_NE(later_message, nullptr);
    later_message->set_service_id(SERVICE + 1);
    auto first_dispatch =
        std::async(std::launch::async, [&] { return transport.emit(first_message); });
    first_entered.get_future().wait();
    auto unsubscribe = std::async(
        std::launch::async, [&] { return subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP); });
    EXPECT_TRUE(wait_for_subscription_count(subscriber, 1));
    auto later_dispatch =
        std::async(std::launch::async, [&] { return transport.emit(later_message); });
    later_entered.get_future().wait();
    first_release.set_value();
    EXPECT_TRUE(first_dispatch.get());
    EXPECT_EQ(unsubscribe.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    later_release.set_value();
    EXPECT_TRUE(later_dispatch.get());
    EXPECT_TRUE(unsubscribe.get());
}

TEST(TransportInjectionRouting,
     ConcurrentUnsubscribersCompleteAndCallbackUnsubscribeDoesNotDeadlock)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    ASSERT_TRUE(
        subscriber.subscribe_eventgroup(SERVICE, 1, GROUP, [&](const events::EventNotification&) {
            entered.set_value();
            released.wait();
            static_cast<void>(subscriber.unsubscribe_eventgroup(SERVICE + 1, 1, GROUP));
        }));
    for (uint16_t service = SERVICE + 1; service < SERVICE + 7; ++service) {
        ASSERT_TRUE(subscriber.subscribe_eventgroup(service, 1, GROUP,
                                                    [](const events::EventNotification&) {}));
    }
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    entered.get_future().wait();
    std::vector<std::future<bool>> unsubscriptions;
    for (uint16_t service = SERVICE; service < SERVICE + 7; ++service) {
        unsubscriptions.push_back(std::async(std::launch::async, [&, service] {
            return subscriber.unsubscribe_eventgroup(service, 1, GROUP);
        }));
    }
    EXPECT_TRUE(wait_for_subscription_count(subscriber, 6));
    release.set_value();
    EXPECT_TRUE(dispatch.get());
    for (auto& unsubscription : unsubscriptions) {
        EXPECT_EQ(unsubscription.wait_for(std::chrono::seconds(1)), std::future_status::ready);
        static_cast<void>(unsubscription.get());
    }
    EXPECT_TRUE(subscriber.get_active_subscriptions().empty());
}

TEST(TransportInjectionRouting, NestedSubscriberCallbacksRecognizeAnOuterDispatch)
{
    FakeTransport outer_transport;
    FakeTransport inner_transport;
    events::EventSubscriber outer(7, outer_transport);
    events::EventSubscriber inner(8, inner_transport);
    outer.set_default_endpoint(PEER.get_address(), PEER.get_port());
    inner.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(outer.initialize());
    ASSERT_TRUE(inner.initialize());
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    ASSERT_TRUE(inner.subscribe_eventgroup(SERVICE, 1, GROUP,
        [&](const events::EventNotification&) {
            EXPECT_TRUE(outer.unsubscribe_eventgroup(SERVICE, 1, GROUP));
        }));
    ASSERT_TRUE(outer.subscribe_eventgroup(SERVICE, 1, GROUP,
        [&](const events::EventNotification&) {
            EXPECT_TRUE(inner_transport.emit(message));
        }));
    EXPECT_TRUE(outer_transport.emit(message));
    EXPECT_TRUE(outer.get_active_subscriptions().empty());
}

#ifdef __cpp_exceptions
TEST(TransportInjectionRouting, ThrowingNotificationReleasesItsDispatchFrame)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(7, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE, 1, GROUP,
        [](const events::EventNotification&) { throw std::runtime_error("notification"); }));
    const auto message = make_dispatch_message<events::EventSubscriber>();
    ASSERT_NE(message, nullptr);
    EXPECT_THROW(transport.emit(message), std::runtime_error);
    auto unsubscribe = std::async(
        std::launch::async, [&] { return subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP); });
    EXPECT_EQ(unsubscribe.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(unsubscribe.get());
}
#endif

}  // namespace
