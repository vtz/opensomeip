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
#include "transport/message_rejection.h"
#include "transport/transport.h"

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
     * @brief Borrow an exclusive, stopped transport instead of creating UDP.
     * @param client_id Client identifier used for event requests.
     * @param transport Must outlive this subscriber; the subscriber manages start/stop.
     * @note Local binding and transport configuration belong to the supplied backend.
     * @see transport::ITransport for the injected-transport lifecycle contract.
     */
    EventSubscriber(uint16_t client_id, transport::ITransport& transport);

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
     * @warning Must not be called from a notification/field callback or its capture
     *          destructor, on either a default or injected transport: shutdown waits
     *          for callback completion. Initiate shutdown from an independent owner
     *          thread and join outstanding API calls before destroying the subscriber.
     */
    void shutdown();

    /**
     * @brief Result of the last transport start/stop (including failed-start cleanup).
     * @return SUCCESS initially, otherwise the last lifecycle outcome; not a receive error.
     * @note Safe to query during initialize/shutdown; object destruction must be serialized.
     */
    Result get_transport_result() const;

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
     * @param filters Stored filter metadata; not enforced on received notifications
     * @return true if subscription request sent, false on error
     * @note Only one (instance_id, eventgroup_id) per service_id is supported by this
     *       subscriber. A distinct key for the same service is rejected without sending
     *       or changing the existing subscription. Renewal of the exact key is allowed.
     *       Wire notifications lack instance/eventgroup identity; endpoint resolution
     *       and optional filters do not supply an unambiguous receive binding.
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
     * @return true if this eventgroup was subscribed (false only if it was not found)
     * @note An external call ALWAYS waits for the notification dispatch, if any, that is
     *       currently delivering *this exact* (service_id, instance_id, eventgroup_id)
     *       subscription -- whether or not this call is the one that actually erased it.
     *       This matters because a reentrant call from that dispatch's own callback (see
     *       below) may have already erased the subscription moments earlier while its
     *       callback -- and the DispatchFrame guarding its captured state -- is still on
     *       the stack; waiting only when @c true was returned would let this call free
     *       that state out from under the still-running callback. The wait is scoped to
     *       this subscription only: an unrelated in-flight dispatch for a different
     *       eventgroup never blocks it, and later notifications for THIS eventgroup
     *       (admitted after removal and cutoff sampling) do not extend the wait.
     *       External unsubscribe calls serialize across this subscriber, including
     *       calls for different subscriptions; the dispatch wait itself is key-scoped.
     *       A call made from this subscriber's own notification callback (reentrant,
     *       detected per-thread) skips the wait to avoid self-deadlock: the current and
     *       any overlapping callbacks may still use their captured state until they
     *       return. Callback capture copy/move/destruction must not re-enter this object.
     * @warning Reentrancy is detected per CALLING THREAD, not per call stack. Calling
     *          unsubscribe_eventgroup() for a subscription from a DIFFERENT thread that a
     *          notification callback for that same subscription is synchronously blocked
     *          on (e.g. the callback offloads work to a worker thread and joins it, and
     *          that worker calls unsubscribe_eventgroup(); or two subscribers'
     *          notification callbacks synchronously unsubscribe each other) is NOT
     *          recognized as reentrant. It takes the external, blocking path and
     *          deadlocks waiting for a dispatch that can never finish. This is prohibited:
     *          only call unsubscribe_eventgroup() for a subscription either from a thread
     *          uninvolved in delivering its notifications, or synchronously from within
     *          that subscription's own notification callback on the delivering thread.
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
     * @note A second request for an already-pending field is rejected without replacing
     *       its callback or sending another request. The field becomes available again
     *       on response or shutdown; no field-request timeout is provided.
     * @note Pending field requests are keyed by (service_id, event_id) only -- NOT by
     *       instance_id. A SOME/IP notification carries no instance id, so the response
     *       cannot be attributed to a specific instance; @p instance_id here only
     *       addresses the initial request message. Consequently a pending field request
     *       for one instance also blocks (as "already pending") a request for the same
     *       service_id/event_id on a DIFFERENT instance, and whichever instance's
     *       notification arrives first satisfies the pending callback.
     */
    bool request_field(uint16_t service_id, uint16_t instance_id, uint16_t event_id,
                      EventNotificationCallback callback);

    /**
     * @brief Store event filter metadata (not enforced on received notifications)
     *
     * @param service_id Service identifier
     * @param instance_id Service instance identifier
     * @param eventgroup_id Event group identifier
     * @param filters New filters to store
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
     * @brief Observe structural rejections of incoming messages.
     *
     * @implements REQ_TRANSPORT_026
     */
    void set_message_rejection_handler(
        platform::Function<void(const transport::MessageRejectionInfo&)> handler);

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
