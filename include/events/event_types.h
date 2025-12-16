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

#ifndef SOMEIP_EVENTS_TYPES_H
#define SOMEIP_EVENTS_TYPES_H

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <string>

namespace someip {
namespace events {

/**
 * @brief Event reliability types
 */
enum class Reliability : uint8_t {
    UNKNOWN,
    UNRELIABLE,      // UDP-based, best-effort delivery
    RELIABLE         // TCP-based, guaranteed delivery
};

/**
 * @brief Event notification types
 */
enum class NotificationType : uint8_t {
    UNKNOWN,
    PERIODIC,        // Regular periodic notifications
    ON_CHANGE,       // Notifications when value changes
    ON_CHANGE_WITH_FILTER,  // Notifications with filter criteria
    POLLING          // Client polls for updates
};

/**
 * @brief Event result codes
 */
enum class EventResult : uint8_t {
    SUCCESS,
    EVENT_NOT_FOUND,
    SUBSCRIPTION_FAILED,
    NETWORK_ERROR,
    TIMEOUT,
    INVALID_PARAMETERS,
    NOT_AUTHORIZED
};

/**
 * @brief Event subscription state
 */
enum class SubscriptionState : uint8_t {
    REQUESTED,
    SUBSCRIBED,
    PENDING,
    REJECTED,
    EXPIRED
};

/**
 * @brief Event subscription information
 */
struct EventSubscription {
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint16_t event_id{0};
    uint16_t eventgroup_id{0};
    SubscriptionState state{SubscriptionState::REQUESTED};
    Reliability reliability{Reliability::UNKNOWN};
    NotificationType notification_type{NotificationType::UNKNOWN};
    std::chrono::milliseconds cycle_time{0};  // For periodic events
    std::chrono::steady_clock::time_point last_notification{std::chrono::steady_clock::now()};

    EventSubscription(uint16_t svc_id = 0, uint16_t inst_id = 0, uint16_t evt_id = 0, uint16_t eg_id = 0)
        : service_id(svc_id), instance_id(inst_id), event_id(evt_id), eventgroup_id(eg_id) {
        last_notification = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Event notification data
 */
struct EventNotification {
    uint16_t service_id{0};
    uint16_t instance_id{0};
    uint16_t event_id{0};
    uint16_t client_id{0};
    uint16_t session_id{0};
    std::vector<uint8_t> event_data;
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};

    EventNotification(uint16_t svc_id = 0, uint16_t inst_id = 0, uint16_t evt_id = 0)
        : service_id(svc_id), instance_id(inst_id), event_id(evt_id) {
        timestamp = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Event configuration
 */
struct EventConfig {
    uint16_t event_id{0};
    uint16_t eventgroup_id{0};
    Reliability reliability{Reliability::UNKNOWN};
    NotificationType notification_type{NotificationType::UNKNOWN};
    std::chrono::milliseconds cycle_time{1000};  // Default 1 second
    bool is_field{false};  // true for fields, false for events
    std::string event_name;
};

/**
 * @brief Event filter for selective notifications
 */
struct EventFilter {
    uint16_t event_id;
    std::vector<uint8_t> filter_data;
    bool operator==(const EventFilter& other) const {
        return event_id == other.event_id && filter_data == other.filter_data;
    }
};

/**
 * @brief Event callback types
 */
using EventNotificationCallback = std::function<void(const EventNotification&)>;
using SubscriptionStatusCallback = std::function<void(uint16_t event_id, SubscriptionState state)>;

/**
 * @brief Event publication policies
 */
enum class PublicationPolicy : uint8_t {
    IMMEDIATE,      // Publish immediately when value changes
    CYCLIC,         // Publish at regular intervals
    ON_REQUEST,     // Publish only when requested
    TRIGGERED       // Publish when triggered by external event
};

} // namespace events
} // namespace someip

#endif // SOMEIP_EVENTS_TYPES_H
