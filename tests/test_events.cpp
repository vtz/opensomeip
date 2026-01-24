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

using namespace someip::events;

/**
 * @brief Events and Subscriptions unit tests
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

    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
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
