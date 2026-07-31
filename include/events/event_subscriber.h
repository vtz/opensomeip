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

#ifndef SOMEIP_EVENTS_SUBSCRIBER_H
#define SOMEIP_EVENTS_SUBSCRIBER_H

#include "event_types.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "transport/endpoint.h"

#ifdef SOMEIP_STATIC_ALLOC
#include "static_config.h"
#else
#include <memory>
#endif

namespace someip::events {

/**
 * @brief Forward declaration
 */
class EventSubscriberImpl;

/**
 * @brief SOME/IP Event Subscriber
 *
 * This interface allows clients to subscribe to events and field notifications
 * from services and receive notifications when events occur.
 */
class EventSubscriber {
public:
    /**
     * @brief Constructor
     * @param client_id Client identifier
     */
    explicit EventSubscriber(uint16_t client_id);

    /**
     * @brief Destructor
     */
    ~EventSubscriber();

    // Delete copy and move operations
    EventSubscriber(const EventSubscriber&) = delete;
    EventSubscriber& operator=(const EventSubscriber&) = delete;
    EventSubscriber(EventSubscriber&&) = delete;
    EventSubscriber& operator=(EventSubscriber&&) = delete;

    /**
     * @brief Initialize the event subscriber
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the event subscriber
     */
    void shutdown();

    /**
     * @brief Set the default service endpoint used when no resolver is configured.
     * @param address Service IP address
     * @param port    Service port
     */
    void set_default_endpoint(const platform::String<>& address, uint16_t port);

    /**
     * @brief Set a resolver function that maps (service_id, instance_id) to an endpoint.
     * @param resolver Callable returning an Endpoint for the given service+instance
     */
    using EndpointResolver = platform::Function<transport::Endpoint(uint16_t, uint16_t)>;
    void set_endpoint_resolver(EndpointResolver resolver);

    /**
     * @brief Subscribe to an event group
     *
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     * @param eventgroup_id Event group identifier
     * @param notification_callback Callback for event notifications
     * @param status_callback Callback for subscription status changes
     * @param filters Optional filters for selective notifications
     * @return true if subscription request sent, false on error
     */
    bool subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                            EventNotificationCallback notification_callback,
                            SubscriptionStatusCallback status_callback = nullptr,
                            const platform::Vector<EventFilter>& filters = {});

    /**
     * @brief Unsubscribe from an event group
     *
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     * @param eventgroup_id Event group identifier
     * @return true if unsubscription request sent, false on error
     */
    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id);

    /**
     * @brief Request field value (one-time read)
     *
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     * @param event_id Field identifier
     * @param callback Callback for field value response
     * @return true if request sent, false on error
     */
    bool request_field(uint16_t service_id, uint16_t instance_id, uint16_t event_id,
                      EventNotificationCallback callback);

    /**
     * @brief Set event filter for selective notifications
     *
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     * @param eventgroup_id Event group identifier
     * @param filters New filters to apply
     * @return true if filters updated, false on error
     */
    bool set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                         const platform::Vector<EventFilter>& filters);

    /**
     * @brief Get active subscriptions
     *
     * @return Vector of active event subscriptions
     */
    platform::Vector<EventSubscription> get_active_subscriptions() const;

    /**
     * @brief Get subscription status for an event group
     *
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     * @param eventgroup_id Event group identifier
     * @return Current subscription state
     */
    SubscriptionState get_subscription_status(uint16_t service_id, uint16_t instance_id,
                                            uint16_t eventgroup_id) const;

    /**
     * @brief Check if subscriber is initialized and ready
     *
     * @return true if ready for event subscriptions
     */
    bool is_ready() const;

    /**
     * @brief Get subscriber statistics
     *
     * @return Statistics about event subscriptions
     */
    struct Statistics {
        uint32_t subscriptions_active{0};
        uint32_t notifications_received{0};
        uint32_t subscription_requests_sent{0};
        uint32_t subscription_responses_received{0};
        std::chrono::milliseconds average_response_time{0};
    };
    Statistics get_statistics() const;

private:
#ifdef SOMEIP_STATIC_ALLOC
    alignas(alignof(std::max_align_t)) char impl_storage_[SOMEIP_PIMPL_EVENTSUB_SIZE];
    EventSubscriberImpl* impl() noexcept { return reinterpret_cast<EventSubscriberImpl*>(impl_storage_); }
    const EventSubscriberImpl* impl() const noexcept { return reinterpret_cast<const EventSubscriberImpl*>(impl_storage_); }
#else
    std::unique_ptr<EventSubscriberImpl> impl_;
    EventSubscriberImpl* impl() noexcept { return impl_.get(); }
    const EventSubscriberImpl* impl() const noexcept { return impl_.get(); }
#endif
};

}  // namespace someip::events

#endif // SOMEIP_EVENTS_SUBSCRIBER_H
