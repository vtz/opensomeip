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

#include <chrono>
#include <common/result.h>
#include <events/event_publisher.h>
#include <events/event_subscriber.h>
#include <events/event_types.h>
#include <gtest/gtest.h>
#include <mutex>
#include <someip/message.h>
#include <thread>
#include <transport/endpoint.h>
#include <transport/udp_transport.h>

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "static_pool_init.h"

// winsock.h, pulled in by the UDP transport, defines ERROR as a macro.
#ifdef ERROR
#undef ERROR
#endif

using namespace someip;
using namespace someip::events;

/**
 * @brief Events and Subscriptions unit tests
 *
 * Covers event type/enum definitions and struct construction.
 * Functional event delivery, subscription routing, and session handling
 * tests belong in dedicated integration tests.
 *
 * @tests REQ_ARCH_001
 * @tests feat_req_someip_720
 * @tests feat_req_someip_730
 */
class EventsTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Setup code
    }

    void TearDown() override
    {
        // Cleanup code
    }
};

// Test event types and enums
TEST_F(EventsTest, ReliabilityTypes)
{
    EXPECT_EQ(static_cast<uint8_t>(Reliability::UNKNOWN), 0);
    EXPECT_EQ(static_cast<uint8_t>(Reliability::UNRELIABLE), 1);
    EXPECT_EQ(static_cast<uint8_t>(Reliability::RELIABLE), 2);
}

TEST_F(EventsTest, NotificationTypes)
{
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::UNKNOWN), 0);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::PERIODIC), 1);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::ON_CHANGE), 2);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::ON_CHANGE_WITH_FILTER), 3);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::POLLING), 4);
}

TEST_F(EventsTest, SubscriptionStates)
{
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::REQUESTED), 0);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::SUBSCRIBED), 1);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::PENDING), 2);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::REJECTED), 3);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::EXPIRED), 4);
}

// Test event configuration
TEST_F(EventsTest, EventConfigConstruction)
{
    EventConfig config;

    EXPECT_EQ(config.event_id, 0u);
    EXPECT_EQ(config.eventgroup_id, 0u);
    EXPECT_EQ(config.reliability, Reliability::UNKNOWN);
    EXPECT_EQ(config.notification_type, NotificationType::UNKNOWN);
    EXPECT_EQ(config.cycle_time, std::chrono::milliseconds(1000));
    EXPECT_FALSE(config.is_field);
    EXPECT_EQ(config.event_name, "");
}

// Test event subscription
TEST_F(EventsTest, EventSubscriptionConstruction)
{
    EventSubscription subscription(0x1234, 0x0001, 0x8001, 0x0001);

    EXPECT_EQ(subscription.service_id, 0x1234u);
    EXPECT_EQ(subscription.instance_id, 0x0001u);
    EXPECT_EQ(subscription.event_id, 0x8001u);
    EXPECT_EQ(subscription.eventgroup_id, 0x0001u);
    EXPECT_EQ(subscription.state, SubscriptionState::REQUESTED);
    EXPECT_EQ(subscription.reliability, Reliability::UNKNOWN);
    EXPECT_EQ(subscription.notification_type, NotificationType::UNKNOWN);
    EXPECT_EQ(subscription.cycle_time, std::chrono::milliseconds(0));
}

// Test event notification
TEST_F(EventsTest, EventNotificationConstruction)
{
    EventNotification notification(0x1234, 0x0001, 0x8001);

    EXPECT_EQ(notification.service_id, 0x1234u);
    EXPECT_EQ(notification.instance_id, 0x0001u);
    EXPECT_EQ(notification.event_id, 0x8001u);
    EXPECT_EQ(notification.client_id, 0u);
    EXPECT_EQ(notification.session_id, 0u);
    EXPECT_TRUE(notification.event_data.empty());
}

// Test event filter
TEST_F(EventsTest, EventFilterComparison)
{
    EventFilter filter1{0x8001, {0x01, 0x02}};
    EventFilter filter2{0x8001, {0x01, 0x02}};
    EventFilter filter3{0x8002, {0x01, 0x02}};

    EXPECT_TRUE(filter1 == filter2);
    EXPECT_FALSE(filter1 == filter3);
}

// Test publication policies (implicitly tested through enums above)

// Test result codes
TEST_F(EventsTest, EventResultCodes)
{
    EXPECT_EQ(static_cast<int>(EventResult::SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(EventResult::EVENT_NOT_FOUND), 1);
    EXPECT_EQ(static_cast<int>(EventResult::SUBSCRIPTION_FAILED), 2);
    EXPECT_EQ(static_cast<int>(EventResult::NETWORK_ERROR), 3);
    EXPECT_EQ(static_cast<int>(EventResult::TIMEOUT), 4);
    EXPECT_EQ(static_cast<int>(EventResult::INVALID_PARAMETERS), 5);
}

// Test event configuration with different types
TEST_F(EventsTest, EventConfigPeriodic)
{
    EventConfig config;
    config.event_id = 0x8001;
    config.eventgroup_id = 0x0001;
    config.reliability = Reliability::UNRELIABLE;
    config.notification_type = NotificationType::PERIODIC;
    config.cycle_time = std::chrono::milliseconds(500);
    config.is_field = false;
    config.event_name = "PeriodicSensor";

    EXPECT_EQ(config.event_id, 0x8001u);
    EXPECT_EQ(config.notification_type, NotificationType::PERIODIC);
    EXPECT_EQ(config.cycle_time, std::chrono::milliseconds(500));
    EXPECT_FALSE(config.is_field);
    EXPECT_EQ(config.event_name, "PeriodicSensor");
}

TEST_F(EventsTest, EventConfigOnChange)
{
    EventConfig config;
    config.event_id = 0x8002;
    config.eventgroup_id = 0x0001;
    config.reliability = Reliability::RELIABLE;
    config.notification_type = NotificationType::ON_CHANGE;
    config.is_field = true;
    config.event_name = "OnChangeField";

    EXPECT_EQ(config.event_id, 0x8002u);
    EXPECT_EQ(config.notification_type, NotificationType::ON_CHANGE);
    EXPECT_TRUE(config.is_field);
    EXPECT_EQ(config.event_name, "OnChangeField");
}

// Test event subscription state transitions
TEST_F(EventsTest, SubscriptionStateTransitions)
{
    EventSubscription subscription(0x1234, 0x0001, 0x8001, 0x0001);

    // Initial state
    EXPECT_EQ(subscription.state, SubscriptionState::REQUESTED);

    // Simulate state changes
    subscription.state = SubscriptionState::PENDING;
    EXPECT_EQ(subscription.state, SubscriptionState::PENDING);

    subscription.state = SubscriptionState::SUBSCRIBED;
    EXPECT_EQ(subscription.state, SubscriptionState::SUBSCRIBED);

    subscription.state = SubscriptionState::EXPIRED;
    EXPECT_EQ(subscription.state, SubscriptionState::EXPIRED);
}

// Test event notification data handling
TEST_F(EventsTest, EventNotificationData)
{
    EventNotification notification(0x1234, 0x0001, 0x8001);

    platform::ByteBuffer test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    notification.event_data = test_data;
    notification.client_id = 0xABCD;
    notification.session_id = 0x1234;

    EXPECT_EQ(notification.event_data.size(), 5u);
    EXPECT_EQ(notification.event_data[0], 0x01);
    EXPECT_EQ(notification.event_data[4], 0x05);
    EXPECT_EQ(notification.client_id, 0xABCDu);
    EXPECT_EQ(notification.session_id, 0x1234u);
}

// Test event filter with different data
TEST_F(EventsTest, EventFilterComplex)
{
    EventFilter filter;
    filter.event_id = 0x8001;
    filter.filter_data = {0xFF, 0x00, 0xAA, 0x55};

    EXPECT_EQ(filter.event_id, 0x8001u);
    EXPECT_EQ(filter.filter_data.size(), 4u);
    EXPECT_EQ(filter.filter_data[0], 0xFF);
    EXPECT_EQ(filter.filter_data[3], 0x55);
}

// Test publication policy enum values (implicitly tested above)

// Test that event configurations can be copied and compared
TEST_F(EventsTest, EventConfigCopy)
{
    EventConfig config1;
    config1.event_id = 0x8001;
    config1.eventgroup_id = 0x0001;
    config1.reliability = Reliability::UNRELIABLE;
    config1.notification_type = NotificationType::PERIODIC;
    config1.cycle_time = std::chrono::milliseconds(1000);
    config1.is_field = false;
    config1.event_name = "TestEvent";

    EventConfig config2 = config1;  // Copy

    EXPECT_EQ(config2.event_id, config1.event_id);
    EXPECT_EQ(config2.eventgroup_id, config1.eventgroup_id);
    EXPECT_EQ(config2.reliability, config1.reliability);
    EXPECT_EQ(config2.notification_type, config1.notification_type);
    EXPECT_EQ(config2.cycle_time, config1.cycle_time);
    EXPECT_EQ(config2.is_field, config1.is_field);
    EXPECT_EQ(config2.event_name, config1.event_name);
}

// ============================================================================
// Subscription TTL Enforcement Tests (Issue #266)
//
// These tests verify that the server-side EventPublisher correctly tracks
// subscription TTL and stops considering subscribers whose TTL has expired.
// ============================================================================

class EventsSubscriptionTTLTest : public ::testing::Test {
   protected:
    static constexpr uint16_t TEST_SERVICE_ID = 0x1234;
    static constexpr uint16_t TEST_INSTANCE_ID = 0x0001;
    static constexpr uint16_t TEST_EVENTGROUP_ID = 0x0001;
    static constexpr uint16_t TEST_CLIENT_A = 0x0100;
    static constexpr uint16_t TEST_CLIENT_B = 0x0200;

    EventPublisher publisher{TEST_SERVICE_ID, TEST_INSTANCE_ID};

    void SetUp() override
    {
        publisher.set_default_client_endpoint("127.0.0.1", 50000);
    }
};

/**
 * @test_case TC_EVT_TTL_001
 * @brief Subscription with explicit TTL is stored and retrievable.
 */
TEST_F(EventsSubscriptionTTLTest, SubscriptionWithTTLStored)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 3600u));

    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    ASSERT_EQ(subs.size(), 1u);
    EXPECT_EQ(subs[0], TEST_CLIENT_A);
}

/**
 * @test_case TC_EVT_TTL_002
 * @brief Subscription with short TTL expires and is no longer returned.
 *
 * This is the core regression: before the fix, a subscription persisted
 * forever regardless of TTL.
 */
TEST_F(EventsSubscriptionTTLTest, SubscriptionExpiresAfterTTL)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 1u));

    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u) << "Subscription should be active immediately after subscribe";

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 0u) << "Subscription must expire after TTL elapses";
}

/**
 * @test_case TC_EVT_TTL_003
 * @brief TTL=0 means StopSubscribeEventgroup — immediate removal.
 */
TEST_F(EventsSubscriptionTTLTest, TTLZeroMeansStopSubscribe)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 3600u));

    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u);

    publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 0u);

    subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 0u) << "TTL=0 must immediately remove subscription";
}

/**
 * @test_case TC_EVT_TTL_004
 * @brief Re-subscribing before expiry refreshes the TTL.
 */
TEST_F(EventsSubscriptionTTLTest, ResubscribeRefreshesTTL)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 1u));

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 3600u));

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u) << "Re-subscription should have refreshed TTL";
}

/**
 * @test_case TC_EVT_TTL_005
 * @brief cleanup_expired_subscriptions removes only expired entries.
 */
TEST_F(EventsSubscriptionTTLTest, CleanupExpiredSubscriptions)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 1u));
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_B, 3600u));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    size_t removed = publisher.cleanup_expired_subscriptions();
    EXPECT_EQ(removed, 1u);

    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    ASSERT_EQ(subs.size(), 1u);
    EXPECT_EQ(subs[0], TEST_CLIENT_B);
}

/**
 * @test_case TC_EVT_TTL_006
 * @brief TTL=0xFFFFFF (24-bit max) means infinite — never expires.
 */
TEST_F(EventsSubscriptionTTLTest, InfiniteTTLNeverExpires)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 0xFFFFFFu));

    publisher.cleanup_expired_subscriptions();
    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u);
}

/**
 * @test_case TC_EVT_TTL_007
 * @brief Backward-compatible handle_subscription (no TTL) uses infinite TTL.
 */
TEST_F(EventsSubscriptionTTLTest, BackwardCompatibleSubscriptionIsInfinite)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A));

    publisher.cleanup_expired_subscriptions();
    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u) << "Default (no TTL) subscription must not expire";
}

/**
 * @test_case TC_EVT_TTL_008
 * @brief Multiple subscribers with different TTLs expire independently.
 */
TEST_F(EventsSubscriptionTTLTest, MultipleSubscribersDifferentTTL)
{
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 1u));
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_B, 10u));

    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 2u);

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    ASSERT_EQ(subs.size(), 1u) << "Only short-TTL subscriber should have expired";
    EXPECT_EQ(subs[0], TEST_CLIENT_B);
}

/**
 * @test_case TC_EVT_TTL_009
 * @brief TTL=0 for a non-existent subscription returns false.
 */
TEST_F(EventsSubscriptionTTLTest, StopSubscribeNonExistentReturnsFalse)
{
    bool result = publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 0u);
    EXPECT_FALSE(result) << "StopSubscribe for unknown client should return false";
}

/**
 * @test_case TC_EVT_FIELD_001
 * @tests REQ_MSG_125
 * @brief A new subscriber receives the current field value after Subscribe.
 */
TEST_F(EventsTest, EventPublisherSendsInitialFieldOnNewSubscription)
{
    transport::UdpTransportConfig rx_cfg;
    rx_cfg.blocking = false;
    transport::UdpTransport rx(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(rx.start(), Result::SUCCESS);
    const uint16_t port = rx.get_local_endpoint().get_port();

    EventPublisher publisher(0x1234, 0x0001);
    publisher.set_default_client_endpoint("127.0.0.1", port);
    ASSERT_TRUE(publisher.initialize());

    EventConfig cfg;
    cfg.event_id = 0x8001;
    cfg.eventgroup_id = 0x0001;
    cfg.is_field = true;
    cfg.notification_type = NotificationType::ON_CHANGE;
    ASSERT_TRUE(publisher.register_event(cfg));

    platform::ByteBuffer payload;
    payload.push_back(0x11);
    payload.push_back(0x22);
    payload.push_back(0x33);
    ASSERT_TRUE(publisher.publish_field(0x8001, payload));

    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0100, 3600u));

    bool got = false;
    platform::ByteBuffer received;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    while (std::chrono::steady_clock::now() < deadline && !got) {
        auto msg = rx.receive_message();
        if (!msg) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (msg->get_method_id() == 0x8001) {
            received = msg->get_payload();
            got = true;
        }
    }

    publisher.shutdown();
    rx.stop();
    ASSERT_TRUE(got);
    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], 0x11);
    EXPECT_EQ(received[1], 0x22);
    EXPECT_EQ(received[2], 0x33);
}

/**
 * @test_case TC_EVT_FIELD_003
 * @tests REQ_MSG_125
 * @brief Periodic republish of a field keeps the cached value.
 */
TEST_F(EventsTest, PeriodicFieldPublishKeepsCachedValue)
{
    transport::UdpTransportConfig rx_cfg;
    rx_cfg.blocking = false;
    transport::UdpTransport rx(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(rx.start(), Result::SUCCESS);
    const uint16_t port = rx.get_local_endpoint().get_port();

    EventPublisher publisher(0x1234, 0x0001);
    publisher.set_default_client_endpoint("127.0.0.1", port);
    ASSERT_TRUE(publisher.initialize());

    EventConfig cfg;
    cfg.event_id = 0x8001;
    cfg.eventgroup_id = 0x0001;
    cfg.is_field = true;
    cfg.notification_type = NotificationType::PERIODIC;
    cfg.cycle_time = std::chrono::milliseconds(100);
    ASSERT_TRUE(publisher.register_event(cfg));

    platform::ByteBuffer payload;
    payload.push_back(0xAB);
    payload.push_back(0xCD);
    ASSERT_TRUE(publisher.publish_field(0x8001, payload));
    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0100, 3600u));

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    transport::UdpTransport rx2(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(rx2.start(), Result::SUCCESS);
    publisher.set_default_client_endpoint("127.0.0.1", rx2.get_local_endpoint().get_port());
    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0200, 3600u));

    bool got = false;
    platform::ByteBuffer received;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    while (std::chrono::steady_clock::now() < deadline && !got) {
        auto msg = rx2.receive_message();
        if (!msg) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (msg->get_method_id() == 0x8001) {
            received = msg->get_payload();
            got = true;
        }
    }

    publisher.shutdown();
    rx.stop();
    rx2.stop();
    ASSERT_TRUE(got);
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0], 0xAB);
    EXPECT_EQ(received[1], 0xCD);
}

/**
 * @test_case TC_EVT_FIELD_002
 * @tests REQ_MSG_125
 * @brief TTL refresh of an existing client_id does not resend the initial field burst.
 */
TEST_F(EventsTest, EventPublisherNoDuplicateInitialOnRefresh)
{
    transport::UdpTransportConfig rx_cfg;
    rx_cfg.blocking = false;
    transport::UdpTransport rx(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(rx.start(), Result::SUCCESS);
    const uint16_t port = rx.get_local_endpoint().get_port();

    EventPublisher publisher(0x1234, 0x0001);
    publisher.set_default_client_endpoint("127.0.0.1", port);
    ASSERT_TRUE(publisher.initialize());

    EventConfig cfg;
    cfg.event_id = 0x8001;
    cfg.eventgroup_id = 0x0001;
    cfg.is_field = true;
    ASSERT_TRUE(publisher.register_event(cfg));

    platform::ByteBuffer payload;
    payload.push_back(0xAB);
    ASSERT_TRUE(publisher.publish_field(0x8001, payload));
    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0100, 3600u));

    int count = 0;
    auto drain_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
    while (std::chrono::steady_clock::now() < drain_until) {
        auto msg = rx.receive_message();
        if (msg && msg->get_method_id() == 0x8001) {
            ++count;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    ASSERT_GE(count, 1);

    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x0100, 3600u));
    int extra = 0;
    auto extra_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < extra_until) {
        auto msg = rx.receive_message();
        if (msg && msg->get_method_id() == 0x8001) {
            ++extra;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    publisher.shutdown();
    rx.stop();
    EXPECT_EQ(extra, 0) << "TTL refresh must not duplicate the initial field notification";
}

namespace {

MessagePtr wait_for_method(transport::UdpTransport& rx, uint16_t method_id,
                           std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        MessagePtr msg = rx.receive_message();
        if (msg && msg->get_method_id() == method_id) {
            return msg;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return nullptr;
}

int count_messages(transport::UdpTransport& rx, std::chrono::milliseconds timeout)
{
    int count = 0;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (rx.receive_message()) {
            ++count;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    return count;
}

transport::UdpTransportConfig nonblocking_udp_config()
{
    transport::UdpTransportConfig cfg;
    cfg.blocking = false;
    cfg.reuse_address = true;
    return cfg;
}

}  // namespace

/**
 * @test_case TC_TRANSPORT_014_UNRESOLVED
 * @tests REQ_TRANSPORT_014
 * @brief An unconfigured subscriber fails before send and emits nothing to the SD port.
 */
TEST_F(EventsTest, UnresolvedDestinationFailsBeforeSend)
{
    const uint16_t sd_port = transport::SOMEIP_SD_MULTICAST_ENDPOINT.get_port();
    transport::UdpTransport sd_listener(transport::Endpoint("127.0.0.1", sd_port),
                                        nonblocking_udp_config());
    ASSERT_EQ(sd_listener.start(), Result::SUCCESS);

    EventSubscriber subscriber(0x0001);
    ASSERT_TRUE(subscriber.initialize());

    EXPECT_EQ(
        subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0001, [](const EventNotification&) {}),
        Result::INVALID_ENDPOINT);
    EXPECT_FALSE(subscriber.request_field(0x1234, 0x0001, 0x8001, [](const EventNotification&) {}));
    EXPECT_TRUE(subscriber.get_active_subscriptions().empty());

    // Port 0 is not a remote application port.
    subscriber.set_default_endpoint("127.0.0.1", 0);
    EXPECT_EQ(
        subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0001, [](const EventNotification&) {}),
        Result::INVALID_ENDPOINT);
    EXPECT_FALSE(subscriber.request_field(0x1234, 0x0001, 0x8001, [](const EventNotification&) {}));

    subscriber.set_endpoint_resolver([](uint16_t, uint16_t) { return transport::Endpoint(); });
    EXPECT_EQ(
        subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0001, [](const EventNotification&) {}),
        Result::INVALID_ENDPOINT);
    EXPECT_FALSE(subscriber.request_field(0x1234, 0x0001, 0x8001, [](const EventNotification&) {}));

    EXPECT_EQ(count_messages(sd_listener, std::chrono::milliseconds(200)), 0);

    subscriber.shutdown();
    sd_listener.stop();
}

/**
 * @test_case TC_TRANSPORT_014_EXPLICIT
 * @tests REQ_TRANSPORT_014
 * @brief An explicit non-SD destination receives subscription and field-request sends.
 */
TEST_F(EventsTest, ExplicitDestinationReceivesApplicationSend)
{
    const uint16_t sd_port = transport::SOMEIP_SD_MULTICAST_ENDPOINT.get_port();
    auto cfg = nonblocking_udp_config();
    transport::UdpTransport app_listener(transport::Endpoint("127.0.0.1", 0), cfg);
    transport::UdpTransport sd_listener(transport::Endpoint("127.0.0.1", sd_port), cfg);
    ASSERT_EQ(app_listener.start(), Result::SUCCESS);
    ASSERT_EQ(sd_listener.start(), Result::SUCCESS);

    const uint16_t app_port = app_listener.get_local_endpoint().get_port();
    ASSERT_NE(app_port, 0);
    ASSERT_NE(app_port, sd_port);

    EventSubscriber subscriber(0x00AB);
    subscriber.set_default_endpoint("127.0.0.1", app_port);
    ASSERT_TRUE(subscriber.initialize());

    EXPECT_EQ(Result::SUCCESS, subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0010,
                                                               [](const EventNotification&) {}));
    MessagePtr subscribe_msg =
        wait_for_method(app_listener, 0x0001, std::chrono::milliseconds(1000));
    ASSERT_NE(subscribe_msg, nullptr);
    EXPECT_EQ(subscribe_msg->get_service_id(), 0x1234);
    EXPECT_EQ(subscribe_msg->get_client_id(), 0x00AB);

    EventSubscriber resolved(0x00CD);
    resolved.set_endpoint_resolver(
        [app_port](uint16_t, uint16_t) { return transport::Endpoint("127.0.0.1", app_port); });
    ASSERT_TRUE(resolved.initialize());
    constexpr uint16_t kGetterMethod = 0x0100;
    EXPECT_TRUE(
        resolved.request_field(0x1234, 0x0001, kGetterMethod, [](const EventNotification&) {}));
    MessagePtr field_msg =
        wait_for_method(app_listener, kGetterMethod, std::chrono::milliseconds(1000));
    ASSERT_NE(field_msg, nullptr);
    EXPECT_EQ(field_msg->get_service_id(), 0x1234);
    EXPECT_EQ(field_msg->get_client_id(), 0x00CD);
    EXPECT_EQ(field_msg->get_message_type(), MessageType::REQUEST);
    EXPECT_TRUE(field_msg->get_payload().empty());
    EXPECT_NE(field_msg->get_method_id(), 0x0003);
    EXPECT_NE(field_msg->get_session_id(), 0);

    EXPECT_EQ(count_messages(sd_listener, std::chrono::milliseconds(200)), 0);

    subscriber.shutdown();
    resolved.shutdown();
    app_listener.stop();
    sd_listener.stop();
}

namespace {

struct FieldWait {
    std::mutex mu;
    bool got{false};
    EventNotification note;
};

EventNotificationCallback field_waiter(FieldWait& wait)
{
    return [&wait](const EventNotification& note) {
        std::lock_guard<std::mutex> const lock(wait.mu);
        wait.note = note;
        wait.got = true;
    };
}

bool wait_until(FieldWait& wait, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> const lock(wait.mu);
            if (wait.got) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::lock_guard<std::mutex> const lock(wait.mu);
    return wait.got;
}

transport::Endpoint loopback_port(uint16_t port)
{
    return transport::Endpoint("127.0.0.1", port);
}

EventConfig field_with_accessors(uint16_t notifier, uint16_t getter, uint16_t setter)
{
    EventConfig cfg;
    cfg.event_id = notifier;
    cfg.eventgroup_id = 0x0001;
    cfg.is_field = true;
    cfg.has_notifier = notifier != 0;
    cfg.has_getter = getter != 0;
    cfg.getter_method_id = getter;
    cfg.has_setter = setter != 0;
    cfg.setter_method_id = setter;
    cfg.notification_type = NotificationType::ON_CHANGE;
    return cfg;
}

}  // namespace

/**
 * @test_case TC_EVT_FIELD_GET_001
 * @tests REQ_MSG_123, REQ_MSG_124
 * @brief Getter and setter requests use the configured method ids.
 *        A getter payload is empty. A setter payload is the value.
 *        Method 0x0003 with an event id in the payload is not the wire format.
 */
TEST_F(EventsTest, FieldGetterAndSetterUseConfiguredMethods)
{
    auto cfg = nonblocking_udp_config();
    transport::UdpTransport app_listener(transport::Endpoint("127.0.0.1", 0), cfg);
    ASSERT_EQ(app_listener.start(), Result::SUCCESS);
    const uint16_t app_port = app_listener.get_local_endpoint().get_port();

    EventSubscriber subscriber(0x00CD);
    subscriber.set_default_endpoint("127.0.0.1", app_port);
    ASSERT_TRUE(subscriber.initialize());

    constexpr uint16_t kGetter = 0x0100;
    constexpr uint16_t kSetter = 0x0101;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, kGetter, [](const EventNotification&) {}));
    MessagePtr getter = wait_for_method(app_listener, kGetter, std::chrono::milliseconds(1000));
    ASSERT_NE(getter, nullptr);
    EXPECT_EQ(getter->get_message_type(), MessageType::REQUEST);
    EXPECT_TRUE(getter->get_payload().empty());
    EXPECT_NE(getter->get_method_id(), 0x0003);
    EXPECT_EQ(getter->get_session_id(), 1);

    platform::ByteBuffer value;
    value.push_back(0x42);
    value.push_back(0x43);
    EXPECT_TRUE(
        subscriber.set_field(0x1234, 0x0001, kSetter, value, [](const EventNotification&) {}));
    MessagePtr setter = wait_for_method(app_listener, kSetter, std::chrono::milliseconds(1000));
    ASSERT_NE(setter, nullptr);
    EXPECT_EQ(setter->get_message_type(), MessageType::REQUEST);
    ASSERT_EQ(setter->get_payload().size(), 2u);
    EXPECT_EQ(setter->get_payload()[0], 0x42);
    EXPECT_EQ(setter->get_payload()[1], 0x43);
    EXPECT_NE(setter->get_payload()[0], 0x80);

    subscriber.shutdown();
    app_listener.stop();
}

/**
 * @test_case TC_EVT_FIELD_REG_001
 * @tests REQ_MSG_126, REQ_MSG_125_E01
 * @brief A field with no getter, setter, or notifier is rejected.
 *        Registering the same notifier again replaces the getter method and keeps the value.
 */
TEST_F(EventsTest, FieldRegistrationRequiresAnAccessorAndReplacesNotifier)
{
    EventPublisher publisher(0x1234, 0x0001);
    ASSERT_TRUE(publisher.initialize());

    EventConfig empty;
    empty.is_field = true;
    empty.has_notifier = false;
    empty.event_id = 0;
    EXPECT_FALSE(publisher.register_event(empty));
    EXPECT_TRUE(publisher.get_registered_events().empty());

    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0100, 0)));
    platform::ByteBuffer stored;
    stored.push_back(0x11);
    ASSERT_TRUE(publisher.publish_field(0x8001, stored));

    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0102, 0)));
    auto events = publisher.get_registered_events();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], 0x8001);

    auto rx_cfg = nonblocking_udp_config();
    transport::UdpTransport client(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(client.start(), Result::SUCCESS);
    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());

    Message stale(MessageId(0x1234, 0x0100), RequestId(0x0001, 0x0001), MessageType::REQUEST,
                  ReturnCode::E_OK);
    ASSERT_EQ(client.send_message(stale, dest), Result::SUCCESS);
    MessagePtr stale_reply = wait_for_method(client, 0x0100, std::chrono::milliseconds(1000));
    ASSERT_NE(stale_reply, nullptr);
    EXPECT_EQ(stale_reply->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(stale_reply->get_return_code(), ReturnCode::E_UNKNOWN_METHOD);

    Message getter(MessageId(0x1234, 0x0102), RequestId(0x0001, 0x0002), MessageType::REQUEST,
                   ReturnCode::E_OK);
    ASSERT_EQ(client.send_message(getter, dest), Result::SUCCESS);
    MessagePtr reply = wait_for_method(client, 0x0102, std::chrono::milliseconds(1000));
    ASSERT_NE(reply, nullptr);
    EXPECT_EQ(reply->get_message_type(), MessageType::RESPONSE);
    ASSERT_EQ(reply->get_payload().size(), 1u);
    EXPECT_EQ(reply->get_payload()[0], 0x11);

    publisher.shutdown();
    client.stop();
}

/**
 * @test_case TC_EVT_FIELD_REG_002
 * @tests REQ_MSG_126, REQ_MSG_125_E01
 * @brief A notifier field and a notifier-less field cannot share a storage id.
 */
TEST_F(EventsTest, FieldRegistrationRejectsTheOtherNotifierKind)
{
    EventPublisher publisher(0x1234, 0x0001);
    ASSERT_TRUE(publisher.initialize());
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x0100, 0x0101, 0)));
    platform::ByteBuffer stored;
    stored.push_back(0x11);
    ASSERT_TRUE(publisher.publish_field(0x0100, stored));
    EXPECT_FALSE(publisher.register_event(field_with_accessors(0, 0x0100, 0x0102)));

    auto events = publisher.get_registered_events();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], 0x0100);

    auto rx_cfg = nonblocking_udp_config();
    transport::UdpTransport client(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(client.start(), Result::SUCCESS);
    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());
    auto ask = [&](uint16_t method) {
        Message request(MessageId(0x1234, method), RequestId(0x0001, method), MessageType::REQUEST,
                        ReturnCode::E_OK);
        EXPECT_EQ(client.send_message(request, dest), Result::SUCCESS);
        return wait_for_method(client, method, std::chrono::milliseconds(1000));
    };

    MessagePtr kept = ask(0x0101);
    ASSERT_NE(kept, nullptr);
    EXPECT_EQ(kept->get_message_type(), MessageType::RESPONSE);
    ASSERT_EQ(kept->get_payload().size(), 1u);
    EXPECT_EQ(kept->get_payload()[0], 0x11);

    MessagePtr stolen = ask(0x0100);
    ASSERT_NE(stolen, nullptr);
    EXPECT_EQ(stolen->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(stolen->get_return_code(), ReturnCode::E_UNKNOWN_METHOD);

    publisher.shutdown();
    client.stop();

    EventPublisher reverse(0x1234, 0x0001);
    ASSERT_TRUE(reverse.initialize());
    ASSERT_TRUE(reverse.register_event(field_with_accessors(0, 0x0200, 0)));
    ASSERT_TRUE(reverse.publish_field(0x0200, stored));
    EXPECT_FALSE(reverse.register_event(field_with_accessors(0x0200, 0x0201, 0)));
    EXPECT_TRUE(reverse.get_registered_events().empty());

    transport::UdpTransport reverse_client(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(reverse_client.start(), Result::SUCCESS);
    const auto reverse_dest = loopback_port(reverse.get_local_endpoint().get_port());
    Message getter(MessageId(0x1234, 0x0200), RequestId(0x0001, 0x0003), MessageType::REQUEST,
                   ReturnCode::E_OK);
    ASSERT_EQ(reverse_client.send_message(getter, reverse_dest), Result::SUCCESS);
    MessagePtr reply = wait_for_method(reverse_client, 0x0200, std::chrono::milliseconds(1000));
    ASSERT_NE(reply, nullptr);
    EXPECT_EQ(reply->get_message_type(), MessageType::RESPONSE);
    ASSERT_EQ(reply->get_payload().size(), 1u);
    EXPECT_EQ(reply->get_payload()[0], 0x11);

    reverse.shutdown();
    reverse_client.stop();
}

/**
 * @test_case TC_EVT_FIELD_REG_003
 * @tests REQ_MSG_126, REQ_MSG_123
 * @brief Updating the named notifier replaces that field. Disabling the notifier
 *        drops the event id and serves the stored value from the new getter.
 */
TEST_F(EventsTest, UpdateKeepsTheNamedFieldWhenTheNotifierIsRemoved)
{
    EventPublisher publisher(0x1234, 0x0001);
    ASSERT_TRUE(publisher.initialize());
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0100, 0x0101)));
    platform::ByteBuffer stored;
    stored.push_back(0x11);
    ASSERT_TRUE(publisher.publish_field(0x8001, stored));
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0, 0x0102, 0)));
    platform::ByteBuffer other;
    other.push_back(0x22);
    ASSERT_TRUE(publisher.publish_field(0x0102, other));

    EXPECT_FALSE(publisher.update_event_config(0x8001, field_with_accessors(0, 0x0102, 0x0103)));
    auto still = publisher.get_registered_events();
    ASSERT_EQ(still.size(), 1u);
    EXPECT_EQ(still[0], 0x8001);

    ASSERT_TRUE(publisher.update_event_config(0x8001, field_with_accessors(0, 0x0106, 0x0107)));
    EXPECT_TRUE(publisher.get_registered_events().empty());

    auto rx_cfg = nonblocking_udp_config();
    transport::UdpTransport client(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(client.start(), Result::SUCCESS);
    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());
    auto ask = [&](uint16_t method) {
        Message request(MessageId(0x1234, method), RequestId(0x0001, method), MessageType::REQUEST,
                        ReturnCode::E_OK);
        EXPECT_EQ(client.send_message(request, dest), Result::SUCCESS);
        return wait_for_method(client, method, std::chrono::milliseconds(1000));
    };

    MessagePtr moved = ask(0x0106);
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(moved->get_message_type(), MessageType::RESPONSE);
    ASSERT_EQ(moved->get_payload().size(), 1u);
    EXPECT_EQ(moved->get_payload()[0], 0x11);

    MessagePtr old_getter = ask(0x0100);
    ASSERT_NE(old_getter, nullptr);
    EXPECT_EQ(old_getter->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(old_getter->get_return_code(), ReturnCode::E_UNKNOWN_METHOD);

    MessagePtr untouched = ask(0x0102);
    ASSERT_NE(untouched, nullptr);
    EXPECT_EQ(untouched->get_message_type(), MessageType::RESPONSE);
    ASSERT_EQ(untouched->get_payload().size(), 1u);
    EXPECT_EQ(untouched->get_payload()[0], 0x22);

    publisher.shutdown();
    client.stop();
}

/**
 * @test_case TC_EVT_FIELD_GET_002
 * @tests REQ_MSG_123, REQ_MSG_123_E01, REQ_MSG_124, REQ_MSG_124_E01
 * @brief The publisher answers a getter with the stored value (type 0x80) and a setter
 *        with the value it stored. An unknown method and a rejected setter are errors.
 */
TEST_F(EventsTest, FieldGetterAndSetterRoundTrip)
{
    EventPublisher publisher(0x1234, 0x0001);
    publisher.set_field_setter(
        [](uint16_t, const platform::ByteBuffer& requested, platform::ByteBuffer& accepted) {
            if (!requested.empty() && requested[0] == 0xFF) {
                accepted = requested;
                accepted[0] = 0x7F;
                return FieldSetOutcome::ACCEPTED;
            }
            if (!requested.empty() && requested[0] == 0x00) {
                return FieldSetOutcome::REJECTED;
            }
            accepted = requested;
            return FieldSetOutcome::ACCEPTED;
        });
    ASSERT_TRUE(publisher.initialize());
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0100, 0x0101)));

    platform::ByteBuffer initial;
    initial.push_back(0x11);
    ASSERT_TRUE(publisher.publish_field(0x8001, initial));

    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());
    EventSubscriber subscriber(0x00AB);
    subscriber.set_default_endpoint(dest.get_address(), dest.get_port());
    ASSERT_TRUE(subscriber.initialize());

    auto rx_cfg = nonblocking_udp_config();
    transport::UdpTransport notifier_rx(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(notifier_rx.start(), Result::SUCCESS);
    publisher.set_default_client_endpoint("127.0.0.1", notifier_rx.get_local_endpoint().get_port());
    ASSERT_TRUE(publisher.handle_subscription(0x0001, 0x00AB, 3600u));
    ASSERT_NE(wait_for_method(notifier_rx, 0x8001, std::chrono::milliseconds(1000)), nullptr);

    FieldWait got;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, 0x0100, field_waiter(got)));
    ASSERT_TRUE(wait_until(got, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(got.mu);
        EXPECT_EQ(got.note.message_type, MessageType::RESPONSE);
        EXPECT_EQ(got.note.return_code, ReturnCode::E_OK);
        EXPECT_EQ(got.note.event_id, 0x0100);
        ASSERT_EQ(got.note.event_data.size(), 1u);
        EXPECT_EQ(got.note.event_data[0], 0x11);
    }

    platform::ByteBuffer adapt;
    adapt.push_back(0xFF);
    FieldWait set_wait;
    EXPECT_TRUE(subscriber.set_field(0x1234, 0x0001, 0x0101, adapt, field_waiter(set_wait)));
    ASSERT_TRUE(wait_until(set_wait, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(set_wait.mu);
        EXPECT_EQ(set_wait.note.message_type, MessageType::RESPONSE);
        EXPECT_EQ(set_wait.note.return_code, ReturnCode::E_OK);
        ASSERT_EQ(set_wait.note.event_data.size(), 1u);
        EXPECT_EQ(set_wait.note.event_data[0], 0x7F);
    }
    MessagePtr notified = wait_for_method(notifier_rx, 0x8001, std::chrono::milliseconds(1000));
    ASSERT_NE(notified, nullptr);
    EXPECT_EQ(notified->get_message_type(), MessageType::NOTIFICATION);
    ASSERT_EQ(notified->get_payload().size(), 1u);
    EXPECT_EQ(notified->get_payload()[0], 0x7F);

    platform::ByteBuffer rejected;
    rejected.push_back(0x00);
    FieldWait reject_wait;
    EXPECT_TRUE(subscriber.set_field(0x1234, 0x0001, 0x0101, rejected, field_waiter(reject_wait)));
    ASSERT_TRUE(wait_until(reject_wait, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(reject_wait.mu);
        EXPECT_EQ(reject_wait.note.message_type, MessageType::ERROR);
        EXPECT_EQ(reject_wait.note.return_code, ReturnCode::E_NOT_OK);
        ASSERT_EQ(reject_wait.note.event_data.size(), 1u);
        EXPECT_EQ(reject_wait.note.event_data[0], 0x7F);
    }

    transport::UdpTransport probe(transport::Endpoint("0.0.0.0", 0), rx_cfg);
    ASSERT_EQ(probe.start(), Result::SUCCESS);
    Message unknown(MessageId(0x1234, 0x01FF), RequestId(0x0002, 0x0009), MessageType::REQUEST,
                    ReturnCode::E_OK);
    ASSERT_EQ(probe.send_message(unknown, dest), Result::SUCCESS);
    MessagePtr unknown_reply = wait_for_method(probe, 0x01FF, std::chrono::milliseconds(1000));
    ASSERT_NE(unknown_reply, nullptr);
    EXPECT_EQ(unknown_reply->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(unknown_reply->get_return_code(), ReturnCode::E_UNKNOWN_METHOD);

    Message stuffed(MessageId(0x1234, 0x0100), RequestId(0x0002, 0x000A), MessageType::REQUEST,
                    ReturnCode::E_OK);
    platform::ByteBuffer event_id_bytes;
    event_id_bytes.push_back(0x80);
    event_id_bytes.push_back(0x01);
    stuffed.set_payload(event_id_bytes);
    ASSERT_EQ(probe.send_message(stuffed, dest), Result::SUCCESS);
    MessagePtr stuffed_reply = wait_for_method(probe, 0x0100, std::chrono::milliseconds(1000));
    ASSERT_NE(stuffed_reply, nullptr);
    EXPECT_EQ(stuffed_reply->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(stuffed_reply->get_return_code(), ReturnCode::E_MALFORMED_MESSAGE);

    subscriber.shutdown();
    publisher.shutdown();
    notifier_rx.stop();
    probe.stop();
}

/**
 * @test_case TC_EVT_FIELD_RESERVED_001
 * @brief Subscribe/unsubscribe datagrams must not be treated as field operations.
 *        Method ids 0x0001 and 0x0002 are reserved for subscribe/unsubscribe.
 */
TEST_F(EventsTest, SubscribeUnsubscribeDoNotInterfereWithFields)
{
    EventPublisher publisher(0x1234, 0x0001);
    ASSERT_TRUE(publisher.initialize());
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0100, 0x0101)));

    platform::ByteBuffer initial;
    initial.push_back(0x11);
    ASSERT_TRUE(publisher.publish_field(0x8001, initial));

    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());

    EventSubscriber subscriber(0x00AB);
    subscriber.set_default_endpoint(dest.get_address(), dest.get_port());
    ASSERT_TRUE(subscriber.initialize());

    // Start a pending getter.
    FieldWait getter_wait;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, 0x0100, field_waiter(getter_wait)));

    // subscribe_eventgroup sends method 0x0001 session 0x0001
    ASSERT_EQ(Result::SUCCESS,
              subscriber.subscribe_eventgroup(0x1234, 0x0001, 0x0001,
                                              [](const EventNotification&) {}));
    // unsubscribe_eventgroup sends method 0x0002 session 0x0002
    ASSERT_TRUE(subscriber.unsubscribe_eventgroup(0x1234, 0x0001, 0x0001));

    // Wait a bit to ensure the publisher processed any subscribe/unsubscribe
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // The stored field value must not have changed.
    FieldWait getter2;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, 0x0100, field_waiter(getter2)));
    // The first getter should have completed (response came).
    ASSERT_TRUE(wait_until(getter_wait, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(getter_wait.mu);
        EXPECT_EQ(getter_wait.note.message_type, MessageType::RESPONSE);
        ASSERT_EQ(getter_wait.note.event_data.size(), 1u);
        EXPECT_EQ(getter_wait.note.event_data[0], 0x11);
    }

    subscriber.shutdown();
    publisher.shutdown();
}

/**
 * @test_case TC_EVT_FIELD_RESERVED_002
 * @brief Registering a getter or setter with method id 0x0001 or 0x0002 fails.
 */
TEST_F(EventsTest, ReservedMethodIdsRejectedOnRegistration)
{
    EventPublisher publisher(0x1234, 0x0001);
    ASSERT_TRUE(publisher.initialize());

    EXPECT_FALSE(publisher.register_event(field_with_accessors(0x8001, 0x0001, 0)));
    EXPECT_FALSE(publisher.register_event(field_with_accessors(0x8001, 0, 0x0001)));
    EXPECT_FALSE(publisher.register_event(field_with_accessors(0x8001, 0x0002, 0)));
    EXPECT_FALSE(publisher.register_event(field_with_accessors(0x8001, 0, 0x0002)));

    // Valid registration works
    EXPECT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0003, 0x0004)));

    // update_event_config also rejects reserved ids
    EXPECT_FALSE(publisher.update_event_config(0x8001, field_with_accessors(0x8001, 0x0001, 0)));
    EXPECT_FALSE(publisher.update_event_config(0x8001, field_with_accessors(0x8001, 0, 0x0002)));

    publisher.shutdown();
}

/**
 * @test_case TC_EVT_FIELD_GETTER_UNSET
 * @brief A getter with no stored value returns ERROR / E_NOT_OK and empty payload.
 */
TEST_F(EventsTest, GetterWithNoStoredValueReturnsError)
{
    EventPublisher publisher(0x1234, 0x0001);
    ASSERT_TRUE(publisher.initialize());
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0100, 0x0101)));
    // No publish_field

    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());
    EventSubscriber subscriber(0x00AB);
    subscriber.set_default_endpoint(dest.get_address(), dest.get_port());
    ASSERT_TRUE(subscriber.initialize());

    FieldWait got;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, 0x0100, field_waiter(got)));
    ASSERT_TRUE(wait_until(got, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(got.mu);
        EXPECT_EQ(got.note.message_type, MessageType::ERROR);
        EXPECT_EQ(got.note.return_code, ReturnCode::E_NOT_OK);
        EXPECT_TRUE(got.note.event_data.empty());
    }

    subscriber.shutdown();
    publisher.shutdown();
}

/**
 * @test_case TC_EVT_FIELD_VERSION_001
 * @brief Interface version is stamped on responses and checked on requests.
 */
TEST_F(EventsTest, InterfaceVersionOnFieldRequestsAndResponses)
{
    EventPublisher publisher(0x1234, 0x0001, static_cast<uint8_t>(0x02));
    ASSERT_TRUE(publisher.initialize());
    ASSERT_TRUE(publisher.register_event(field_with_accessors(0x8001, 0x0100, 0x0101)));

    platform::ByteBuffer initial;
    initial.push_back(0xAA);
    ASSERT_TRUE(publisher.publish_field(0x8001, initial));

    const auto dest = loopback_port(publisher.get_local_endpoint().get_port());
    EventSubscriber subscriber(0x00AB);
    subscriber.set_default_endpoint(dest.get_address(), dest.get_port());
    ASSERT_TRUE(subscriber.initialize());

    // Request with matching version 0x02: success
    FieldWait ok_wait;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, 0x0100, field_waiter(ok_wait), 0x02));
    ASSERT_TRUE(wait_until(ok_wait, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(ok_wait.mu);
        EXPECT_EQ(ok_wait.note.message_type, MessageType::RESPONSE);
        EXPECT_EQ(ok_wait.note.return_code, ReturnCode::E_OK);
        EXPECT_EQ(ok_wait.note.interface_version, 0x02);
        ASSERT_EQ(ok_wait.note.event_data.size(), 1u);
        EXPECT_EQ(ok_wait.note.event_data[0], 0xAA);
    }

    // Request with wrong version 0x01: E_WRONG_INTERFACE_VERSION
    FieldWait bad_wait;
    EXPECT_TRUE(subscriber.request_field(0x1234, 0x0001, 0x0100, field_waiter(bad_wait), 0x01));
    ASSERT_TRUE(wait_until(bad_wait, std::chrono::milliseconds(1000)));
    {
        std::lock_guard<std::mutex> const lock(bad_wait.mu);
        EXPECT_EQ(bad_wait.note.message_type, MessageType::ERROR);
        EXPECT_EQ(bad_wait.note.return_code, ReturnCode::E_WRONG_INTERFACE_VERSION);
    }

    subscriber.shutdown();
    publisher.shutdown();
}
