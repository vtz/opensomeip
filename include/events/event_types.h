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

#include <chrono>
#include <cstdint>
#include <memory>

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "someip/types.h"

namespace someip::events {

/**
 * @brief Event reliability types
 */
enum class Reliability : uint8_t {
    UNKNOWN,
    UNRELIABLE,  // UDP-based, best-effort delivery
    RELIABLE     // TCP-based, guaranteed delivery
};

/**
 * @brief Event notification types
 */
enum class NotificationType : uint8_t {
    UNKNOWN,
    PERIODIC,               // Regular periodic notifications
    ON_CHANGE,              // Notifications when value changes
    ON_CHANGE_WITH_FILTER,  // Notifications with filter criteria
    POLLING                 // Client polls for updates
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
enum class SubscriptionState : uint8_t { REQUESTED, SUBSCRIBED, PENDING, REJECTED, EXPIRED };

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

    explicit EventSubscription(uint16_t svc_id = 0, uint16_t inst_id = 0, uint16_t evt_id = 0,
                               uint16_t eg_id = 0)
        : service_id(svc_id), instance_id(inst_id), event_id(evt_id), eventgroup_id(eg_id)
    {
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
    platform::ByteBuffer event_data;
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
    // Message Type and Return Code of a field getter/setter response.
    // Event notifications leave the defaults (NOTIFICATION / E_OK).
    MessageType message_type{MessageType::NOTIFICATION};
    ReturnCode return_code{ReturnCode::E_OK};
    uint8_t interface_version{0x01};

    explicit EventNotification(uint16_t svc_id = 0, uint16_t inst_id = 0, uint16_t evt_id = 0)
        : service_id(svc_id), instance_id(inst_id), event_id(evt_id)
    {
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
    bool is_field{false};                        // true for fields, false for events
    // Field accessors. Ignored unless is_field is true.
    // A field needs at least one of getter, setter, or notifier.
    // has_notifier defaults to true so an existing is_field registration
    // keeps event_id as the notifier event id.
    bool has_getter{false};
    uint16_t getter_method_id{0};
    bool has_setter{false};
    uint16_t setter_method_id{0};
    bool has_notifier{true};
    platform::String<> event_name;
};

/**
 * @brief Eventgroup multicast threshold
 *
 * Threshold 0 unicasts each subscriber. Threshold 1 multicasts only.
 * Above 1, unicast while the active subscriber count is below this value
 * and send one multicast datagram at and above it.
 */
struct EventgroupConfig {
    uint16_t eventgroup_id{0};
    uint16_t multicast_threshold{0};
    platform::String<> multicast_address;
    uint16_t multicast_port{0};
};

/**
 * @brief Application decision for one field setter request.
 *
 * ACCEPTED stores @p accepted, which may differ from the requested bytes.
 * REJECTED leaves the stored value unchanged.
 */
enum class FieldSetOutcome : uint8_t { ACCEPTED, REJECTED };

/**
 * @brief Optional setter adaptation hook.
 *
 * Called without the publisher lock. Must not throw.
 * @p accepted is the value to store when the outcome is ACCEPTED.
 */
using FieldSetter = platform::Function<FieldSetOutcome(uint16_t setter_method_id,
                                                       const platform::ByteBuffer& requested,
                                                       platform::ByteBuffer& accepted)>;

/**
 * @brief Event filter for selective notifications
 * @note Filter metadata is currently stored but not evaluated during event delivery.
 */
struct EventFilter {
    uint16_t event_id{0};
    platform::ByteBuffer filter_data;
    bool operator==(const EventFilter& other) const
    {
        return event_id == other.event_id && filter_data == other.filter_data;
    }
};

/**
 * @brief Event callback types
 */
using EventNotificationCallback = platform::Function<void(const EventNotification&)>;
using SubscriptionStatusCallback =
    platform::Function<void(uint16_t event_id, SubscriptionState state)>;

/**
 * @brief Event publication policies
 */
enum class PublicationPolicy : uint8_t {
    IMMEDIATE,   // Publish immediately when value changes
    CYCLIC,      // Publish at regular intervals
    ON_REQUEST,  // Publish only when requested
    TRIGGERED    // Publish when triggered by external event
};

}  // namespace someip::events

#endif  // SOMEIP_EVENTS_TYPES_H
