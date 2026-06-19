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

#ifndef SOMEIP_EVENTS_PUBLISHER_H
#define SOMEIP_EVENTS_PUBLISHER_H

#include "event_types.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"

#include <memory>

namespace someip::events {

/**
 * @brief Forward declaration
 */
class EventPublisherImpl;

/**
 * @brief SOME/IP Event Publisher
 *
 * This interface allows services to publish events and field notifications
 * to subscribed clients using the SOME/IP event mechanism.
 */
class EventPublisher {
public:
    /**
     * @brief Constructor
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     */
    EventPublisher(uint16_t service_id, uint16_t instance_id);

    /**
     * @brief Destructor
     */
    ~EventPublisher();

    // Delete copy and move operations
    EventPublisher(const EventPublisher&) = delete;
    EventPublisher& operator=(const EventPublisher&) = delete;
    EventPublisher(EventPublisher&&) = delete;
    EventPublisher& operator=(EventPublisher&&) = delete;

    /**
     * @brief Initialize the event publisher
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the event publisher
     */
    void shutdown();

    /**
     * @brief Register an event for publication
     *
     * @param config Event configuration
     * @return true if registered successfully, false on error
     */
    bool register_event(const EventConfig& config);

    /**
     * @brief Unregister an event
     *
     * @param event_id Event identifier to remove
     * @return true if unregistered, false if not found
     */
    bool unregister_event(uint16_t event_id);

    /**
     * @brief Update event configuration
     *
     * @param event_id Event identifier
     * @param config New event configuration
     * @return true if updated, false if event not found
     */
    bool update_event_config(uint16_t event_id, const EventConfig& config);

    /**
     * @brief Publish an event notification
     *
     * @param event_id Event identifier
     * @param data Event data payload
     * @return true if published successfully, false on error
     */
    bool publish_event(uint16_t event_id, const platform::ByteBuffer& data);

    /**
     * @brief Publish a field notification (immediate update)
     *
     * @param event_id Field identifier
     * @param data Field data payload
     * @return true if published successfully, false on error
     */
    bool publish_field(uint16_t event_id, const platform::ByteBuffer& data);

    /**
     * @brief Set the default client endpoint for subscriptions that don't
     *        provide an explicit endpoint (e.g. from SD-discovered addresses).
     * @param address Client IP address
     * @param port    Client port
     */
    void set_default_client_endpoint(const platform::String<>& address, uint16_t port);

    /**
     * @brief Handle event subscription request
     *
     * @param eventgroup_id Event group being subscribed to
     * @param client_id Client identifier
     * @param filters Optional filters for selective notifications
     * @return true if subscription handled, false on error
     */
    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                           const platform::Vector<EventFilter>& filters = {});

    /**
     * @brief Handle event subscription request with explicit TTL
     *
     * Per SOME/IP-SD, the server must track subscription TTL and stop
     * sending events once the TTL expires without renewal.
     *
     * @param eventgroup_id Event group being subscribed to
     * @param client_id Client identifier
     * @param ttl_seconds Subscription TTL in seconds.
     *        0 = StopSubscribeEventgroup (immediate removal).
     *        0xFFFFFF = infinite (never expires).
     * @param filters Optional filters for selective notifications
     * @return true if subscription handled, false on error
     */
    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                           uint32_t ttl_seconds,
                           const platform::Vector<EventFilter>& filters = {});

    /**
     * @brief Handle event unsubscription
     *
     * @param eventgroup_id Event group being unsubscribed from
     * @param client_id Client identifier
     * @return true if unsubscription handled, false on error
     */
    bool handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id);

    /**
     * @brief Remove subscriptions whose TTL has expired.
     *
     * Called automatically from the publish timer, but may also be invoked
     * manually for deterministic testing.
     *
     * @return Number of subscriptions removed
     */
    size_t cleanup_expired_subscriptions();

    /**
     * @brief Get registered events
     *
     * @return Vector of registered event IDs
     */
    platform::Vector<uint16_t> get_registered_events() const;

    /**
     * @brief Get active subscriptions for an event group
     *
     * @param eventgroup_id Event group identifier
     * @return Vector of subscribed client IDs
     */
    platform::Vector<uint16_t> get_subscriptions(uint16_t eventgroup_id) const;

    /**
     * @brief Check if publisher is initialized and ready
     *
     * @return true if ready for event publication
     */
    bool is_ready() const;

    /**
     * @brief Get publisher statistics
     *
     * @return Statistics about event publications
     */
    struct Statistics {
        uint32_t events_registered{0};
        uint32_t notifications_sent{0};
        uint32_t subscriptions_active{0};
        uint32_t subscriptions_rejected{0};
        std::chrono::milliseconds average_publish_time{0};
    };
    Statistics get_statistics() const;

private:
    std::unique_ptr<EventPublisherImpl> impl_;
};

}  // namespace someip::events

#endif // SOMEIP_EVENTS_PUBLISHER_H
