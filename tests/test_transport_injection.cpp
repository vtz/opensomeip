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
#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "events/event_publisher.h"
#include "events/event_subscriber.h"
#include "platform/memory.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_server.h"
#include "someip/message.h"
#include "static_pool_init.h"
#include "transport/transport.h"

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
        std::lock_guard<std::mutex> lock(mutex_);
        messages.push_back(message);
        destinations.push_back(endpoint);
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
        running_ = false;
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
        if (before_dispatch) {
            before_dispatch();
        }
        target->on_message_received(message, sender, get_local_endpoint());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --in_flight_;
        }
        drained_.notify_all();
        return true;
    }

    Result start_result{Result::SUCCESS};
    Result stop_result{Result::SUCCESS};
    Result send_result{Result::SUCCESS};
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> stops{0};
    std::vector<Message> messages;
    std::vector<Endpoint> destinations;
    std::function<void()> before_dispatch;
    std::function<void()> on_stop;

   private:
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable drained_;
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

    // Independent probes, so each map is proven to release its own captures.
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

}  // namespace
