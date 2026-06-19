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

#include <gtest/gtest.h>
#include <events/event_types.h>
#include <events/event_publisher.h>
#include <thread>
#include <chrono>

#include "platform/buffer_pool.h"
#include "platform/containers.h"

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
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

// Test event types and enums
TEST_F(EventsTest, ReliabilityTypes) {
    EXPECT_EQ(static_cast<uint8_t>(Reliability::UNKNOWN), 0);
    EXPECT_EQ(static_cast<uint8_t>(Reliability::UNRELIABLE), 1);
    EXPECT_EQ(static_cast<uint8_t>(Reliability::RELIABLE), 2);
}

TEST_F(EventsTest, NotificationTypes) {
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::UNKNOWN), 0);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::PERIODIC), 1);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::ON_CHANGE), 2);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::ON_CHANGE_WITH_FILTER), 3);
    EXPECT_EQ(static_cast<uint8_t>(NotificationType::POLLING), 4);
}

TEST_F(EventsTest, SubscriptionStates) {
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::REQUESTED), 0);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::SUBSCRIBED), 1);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::PENDING), 2);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::REJECTED), 3);
    EXPECT_EQ(static_cast<uint8_t>(SubscriptionState::EXPIRED), 4);
}

// Test event configuration
TEST_F(EventsTest, EventConfigConstruction) {
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
TEST_F(EventsTest, EventSubscriptionConstruction) {
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
TEST_F(EventsTest, EventNotificationConstruction) {
    EventNotification notification(0x1234, 0x0001, 0x8001);

    EXPECT_EQ(notification.service_id, 0x1234u);
    EXPECT_EQ(notification.instance_id, 0x0001u);
    EXPECT_EQ(notification.event_id, 0x8001u);
    EXPECT_EQ(notification.client_id, 0u);
    EXPECT_EQ(notification.session_id, 0u);
    EXPECT_TRUE(notification.event_data.empty());
}

// Test event filter
TEST_F(EventsTest, EventFilterComparison) {
    EventFilter filter1{0x8001, {0x01, 0x02}};
    EventFilter filter2{0x8001, {0x01, 0x02}};
    EventFilter filter3{0x8002, {0x01, 0x02}};

    EXPECT_TRUE(filter1 == filter2);
    EXPECT_FALSE(filter1 == filter3);
}

// Test publication policies (implicitly tested through enums above)

// Test result codes
TEST_F(EventsTest, EventResultCodes) {
    EXPECT_EQ(static_cast<int>(EventResult::SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(EventResult::EVENT_NOT_FOUND), 1);
    EXPECT_EQ(static_cast<int>(EventResult::SUBSCRIPTION_FAILED), 2);
    EXPECT_EQ(static_cast<int>(EventResult::NETWORK_ERROR), 3);
    EXPECT_EQ(static_cast<int>(EventResult::TIMEOUT), 4);
    EXPECT_EQ(static_cast<int>(EventResult::INVALID_PARAMETERS), 5);
}

// Test event configuration with different types
TEST_F(EventsTest, EventConfigPeriodic) {
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

TEST_F(EventsTest, EventConfigOnChange) {
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
TEST_F(EventsTest, SubscriptionStateTransitions) {
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
TEST_F(EventsTest, EventNotificationData) {
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
TEST_F(EventsTest, EventFilterComplex) {
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
TEST_F(EventsTest, EventConfigCopy) {
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

    void SetUp() override {
        publisher.set_default_client_endpoint("127.0.0.1", 50000);
    }
};

/**
 * @test_case TC_EVT_TTL_001
 * @brief Subscription with explicit TTL is stored and retrievable.
 */
TEST_F(EventsSubscriptionTTLTest, SubscriptionWithTTLStored) {
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
TEST_F(EventsSubscriptionTTLTest, SubscriptionExpiresAfterTTL) {
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
TEST_F(EventsSubscriptionTTLTest, TTLZeroMeansStopSubscribe) {
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
TEST_F(EventsSubscriptionTTLTest, ResubscribeRefreshesTTL) {
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
TEST_F(EventsSubscriptionTTLTest, CleanupExpiredSubscriptions) {
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
TEST_F(EventsSubscriptionTTLTest, InfiniteTTLNeverExpires) {
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 0xFFFFFFu));

    publisher.cleanup_expired_subscriptions();
    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u);
}

/**
 * @test_case TC_EVT_TTL_007
 * @brief Backward-compatible handle_subscription (no TTL) uses infinite TTL.
 */
TEST_F(EventsSubscriptionTTLTest, BackwardCompatibleSubscriptionIsInfinite) {
    EXPECT_TRUE(publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A));

    publisher.cleanup_expired_subscriptions();
    auto subs = publisher.get_subscriptions(TEST_EVENTGROUP_ID);
    EXPECT_EQ(subs.size(), 1u) << "Default (no TTL) subscription must not expire";
}

/**
 * @test_case TC_EVT_TTL_008
 * @brief Multiple subscribers with different TTLs expire independently.
 */
TEST_F(EventsSubscriptionTTLTest, MultipleSubscribersDifferentTTL) {
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
TEST_F(EventsSubscriptionTTLTest, StopSubscribeNonExistentReturnsFalse) {
    bool result = publisher.handle_subscription(TEST_EVENTGROUP_ID, TEST_CLIENT_A, 0u);
    EXPECT_FALSE(result) << "StopSubscribe for unknown client should return false";
}
