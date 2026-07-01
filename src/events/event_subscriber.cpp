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

#include "events/event_subscriber.h"

#include "common/result.h"
#include "events/event_types.h"
#include "platform/thread.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace someip::events {

namespace {
void uint16_to_str(uint16_t val, platform::String<>& out) {
    if (val == 0) {
        out.append("0");
        return;
    }
    std::array<char, 6> digits{};
    int pos = 5;
    while (val > 0) {
        --pos;
        digits[static_cast<size_t>(pos)] = static_cast<char>('0' + (val % 10));
        val /= 10;
    }
    out.append(&digits[static_cast<size_t>(pos)],
               &digits[5]);
}
}  // namespace

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex from platform/thread.h (IWYU false positives in impl).

/**
 * @brief Event Subscriber implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_730
 * @satisfies feat_req_someip_731
 */
class EventSubscriberImpl : public transport::ITransportListener {
public:
    explicit EventSubscriberImpl(uint16_t client_id)
        : client_id_(client_id),
          transport_(std::make_shared<transport::UdpTransport>(
              transport::Endpoint("0.0.0.0", 0))),
          running_(false) {

        transport_->set_listener(this);
    }

    ~EventSubscriberImpl() override
    {
        shutdown();
    }

    EventSubscriberImpl(const EventSubscriberImpl&) = delete;
    EventSubscriberImpl& operator=(const EventSubscriberImpl&) = delete;
    EventSubscriberImpl(EventSubscriberImpl&&) = delete;
    EventSubscriberImpl& operator=(EventSubscriberImpl&&) = delete;

    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_->start() != Result::SUCCESS) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
        if (!running_) {
            return;
        }

        running_ = false;

        // Clear all subscriptions and callbacks
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        subscriptions_.clear();

        transport_->stop();
    }

    /** @implements REQ_MSG_122 */
    bool subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                            EventNotificationCallback notification_callback,
                            SubscriptionStatusCallback status_callback,
                            const platform::Vector<EventFilter>& filters) {

        if (!running_) {
            return false;
        }

        // Create subscription info
        EventSubscription subscription(service_id, instance_id, 0, eventgroup_id);
        subscription.state = SubscriptionState::REQUESTED;

        SubscriptionInfo sub_info;
        sub_info.subscription = subscription;
        sub_info.notification_callback = std::move(notification_callback);
        sub_info.status_callback = std::move(status_callback);
        sub_info.filters = filters;

        // Store subscription
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        const platform::String<> key = make_subscription_key(service_id, instance_id, eventgroup_id);
        subscriptions_[key] = std::move(sub_info);

        const transport::Endpoint service_endpoint = resolve_service_endpoint(service_id, instance_id);
        if (service_endpoint.get_port() == 0) {
            subscriptions_.erase(key);
            return false;
        }

        MessageId const msg_id(service_id, 0x0001);  // Method ID for subscription
        Message subscription_msg(msg_id, RequestId(client_id_, 0x0001), MessageType::REQUEST,
                                 ReturnCode::E_OK);

        // Add subscription data to payload
        platform::ByteBuffer payload;
        payload.push_back(static_cast<uint8_t>((static_cast<uint32_t>(eventgroup_id) >> 8U) & 0xFFU));
        payload.push_back(static_cast<uint8_t>(static_cast<uint32_t>(eventgroup_id) & 0xFFU));
        subscription_msg.set_payload(payload);

        const Result send_result = transport_->send_message(subscription_msg, service_endpoint);
        bool const success = (send_result == Result::SUCCESS);
        if (!success) {
            subscriptions_.erase(key);
        }
        return success;
    }

    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
        if (!running_) {
            return false;
        }

        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        const platform::String<> key = make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return false;
        }

        const transport::Endpoint service_endpoint = resolve_service_endpoint(service_id, instance_id);
        if (service_endpoint.get_port() == 0) {
            return false;
        }

        MessageId const msg_id(service_id, 0x0002);
        Message unsubscription_msg(msg_id, RequestId(client_id_, 0x0002),
                                   MessageType::REQUEST, ReturnCode::E_OK);

        // Add unsubscription data to payload
        platform::ByteBuffer payload;
        payload.push_back(static_cast<uint8_t>((static_cast<uint32_t>(eventgroup_id) >> 8U) & 0xFFU));
        payload.push_back(static_cast<uint8_t>(static_cast<uint32_t>(eventgroup_id) & 0xFFU));
        unsubscription_msg.set_payload(payload);

        const Result result = transport_->send_message(unsubscription_msg, service_endpoint);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }

        // Remove subscription
        subscriptions_.erase(it);
        return true;
    }

    bool request_field(uint16_t service_id, uint16_t instance_id, uint16_t event_id,
                      EventNotificationCallback callback) {

        if (!running_) {
            return false;
        }

        transport::Endpoint service_endpoint;
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            service_endpoint = resolve_service_endpoint(service_id, instance_id);
        }
        if (service_endpoint.get_port() == 0) {
            return false;
        }

        platform::ScopedLock const field_lock(field_requests_mutex_);
        const platform::String<> key = make_field_key(service_id, 0, event_id);
        field_requests_[key] = std::move(callback);

        MessageId const msg_id(service_id, 0x0003);
        Message field_msg(msg_id, RequestId(client_id_, 0x0003), MessageType::REQUEST,
                          ReturnCode::E_OK);

        // Add field ID to payload
        platform::ByteBuffer payload;
        payload.push_back(static_cast<uint8_t>((static_cast<uint32_t>(event_id) >> 8U) & 0xFFU));
        payload.push_back(static_cast<uint8_t>(static_cast<uint32_t>(event_id) & 0xFFU));
        field_msg.set_payload(payload);

        return transport_->send_message(field_msg, service_endpoint) == Result::SUCCESS;
    }

    bool set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                         const platform::Vector<EventFilter>& filters) {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        platform::String<> const key = make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return false;
        }

        it->second.filters = filters;
        return true;
    }

    /** @implements REQ_MSG_123, REQ_MSG_123_E01 */
    platform::Vector<EventSubscription> get_active_subscriptions() const {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        platform::Vector<EventSubscription> result;
        result.reserve(subscriptions_.size());

        for (const auto& pair : subscriptions_) {
            result.push_back(pair.second.subscription);
        }

        return result;
    }

    SubscriptionState get_subscription_status(uint16_t service_id, uint16_t instance_id,
                                            uint16_t eventgroup_id) const {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        platform::String<> const key = make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return SubscriptionState::REQUESTED;
        }

        return it->second.subscription.state;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    EventSubscriber::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return EventSubscriber::Statistics{};
    }

    using EndpointResolver = platform::Function<transport::Endpoint(uint16_t, uint16_t)>;

    void set_endpoint_resolver(EndpointResolver resolver) {
        platform::ScopedLock const lock(subscriptions_mutex_);
        endpoint_resolver_ = std::move(resolver);
    }

    void set_default_endpoint(const platform::String<>& address, uint16_t port) {
        platform::ScopedLock const lock(subscriptions_mutex_);
        default_service_address_ = address;
        default_service_port_ = port;
    }

private:
    struct SubscriptionInfo {
        EventSubscription subscription;
        EventNotificationCallback notification_callback;
        SubscriptionStatusCallback status_callback;
        platform::Vector<EventFilter> filters;
    };

    transport::Endpoint resolve_service_endpoint(uint16_t service_id, uint16_t instance_id) const {
        if (endpoint_resolver_) {
            return endpoint_resolver_(service_id, instance_id);
        }
        if (default_service_address_ == "0.0.0.0" && default_service_port_ == 0) {
            return transport::Endpoint();
        }
        return transport::Endpoint(default_service_address_, default_service_port_);
    }

    platform::String<> make_subscription_key(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) const {
        platform::String<> key;
        uint16_to_str(service_id, key);
        key.append(":");
        uint16_to_str(instance_id, key);
        key.append(":");
        uint16_to_str(eventgroup_id, key);
        return key;
    }

    platform::String<> make_field_key(uint16_t service_id, uint16_t instance_id, uint16_t event_id) {
        platform::String<> key;
        uint16_to_str(service_id, key);
        key.append(":");
        uint16_to_str(instance_id, key);
        key.append(":");
        uint16_to_str(event_id, key);
        return key;
    }

    void on_message_received(MessagePtr message, const transport::Endpoint& /*sender*/) override {
        // Check if this is an event notification
        if (message->get_message_type() != MessageType::NOTIFICATION) {
            return;
        }

        // Check if this is for one of our subscriptions
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

        uint16_t const service_id = message->get_service_id();
        uint16_t const event_id = message->get_method_id();  // Event ID is in method ID field for notifications

        // Find matching subscription (we need to check all subscriptions for this service)
        for (auto& sub_pair : subscriptions_) {
            auto& sub_info = sub_pair.second;
            if (sub_info.subscription.service_id == service_id) {
                // Create event notification
                EventNotification notification(service_id, sub_info.subscription.instance_id, event_id);
                notification.client_id = message->get_client_id();
                notification.session_id = message->get_session_id();
                notification.event_data = message->get_payload();

                // Call notification callback
                if (sub_info.notification_callback) {
                    sub_info.notification_callback(notification);
                }

                // Update subscription state
                sub_info.subscription.state = SubscriptionState::SUBSCRIBED;
                sub_info.subscription.last_notification = std::chrono::steady_clock::now();

                break;
            }
        }

        // Check if this is a field response
        platform::ScopedLock const field_lock(field_requests_mutex_);
        platform::String<> const field_key = make_field_key(service_id, 0, event_id);  // Simplified

        auto field_it = field_requests_.find(field_key);
        if (field_it != field_requests_.end()) {
            EventNotification notification(service_id, 0, event_id);
            notification.event_data = message->get_payload();

            if (field_it->second) {
                field_it->second(notification);
            }

            field_requests_.erase(field_it);
        }
    }

    void on_connection_lost(const transport::Endpoint& /*endpoint*/) override {
        // Handle service disconnection
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

        for (auto& sub_pair : subscriptions_) {
            auto& sub_info = sub_pair.second;
            if (sub_info.subscription.state == SubscriptionState::SUBSCRIBED) {
                sub_info.subscription.state = SubscriptionState::PENDING;
                // TODO: Attempt to reconnect
            }
        }
    }

    void on_connection_established(const transport::Endpoint& /*endpoint*/) override {
        // Handle service reconnection
    }

    void on_error(Result /*error*/) override {
        // Handle transport errors
    }

    uint16_t client_id_;
    platform::String<> default_service_address_{"0.0.0.0"};
    uint16_t default_service_port_{0};
    EndpointResolver endpoint_resolver_;
    std::shared_ptr<transport::UdpTransport> transport_;

    platform::UnorderedMap<platform::String<>, SubscriptionInfo> subscriptions_;
    mutable platform::Mutex subscriptions_mutex_;  // Lock order: acquire before field_requests_mutex_

    platform::UnorderedMap<platform::String<>, EventNotificationCallback> field_requests_;
    mutable platform::Mutex field_requests_mutex_;  // Lock order: acquire after subscriptions_mutex_

    std::atomic<bool> running_;
};

// EventSubscriber implementation
EventSubscriber::EventSubscriber(uint16_t client_id)
    : impl_(std::make_unique<EventSubscriberImpl>(client_id)) {
}

EventSubscriber::~EventSubscriber() = default;

void EventSubscriber::set_default_endpoint(const platform::String<>& address, uint16_t port) {
    impl_->set_default_endpoint(address, port);
}

void EventSubscriber::set_endpoint_resolver(EndpointResolver resolver) {
    impl_->set_endpoint_resolver(std::move(resolver));
}

bool EventSubscriber::initialize() {
    return impl_->initialize();
}

void EventSubscriber::shutdown() {
    impl_->shutdown();
}

bool EventSubscriber::subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                         EventNotificationCallback notification_callback,
                                         SubscriptionStatusCallback status_callback,
                                         const platform::Vector<EventFilter>& filters) {
    return impl_->subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                                     std::move(notification_callback), std::move(status_callback), filters);
}

bool EventSubscriber::unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
    return impl_->unsubscribe_eventgroup(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::request_field(uint16_t service_id, uint16_t instance_id, uint16_t event_id,
                                   EventNotificationCallback callback) {
    return impl_->request_field(service_id, instance_id, event_id, std::move(callback));
}

bool EventSubscriber::set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                      const platform::Vector<EventFilter>& filters) {
    return impl_->set_event_filters(service_id, instance_id, eventgroup_id, filters);
}

platform::Vector<EventSubscription> EventSubscriber::get_active_subscriptions() const {
    return impl_->get_active_subscriptions();
}

SubscriptionState EventSubscriber::get_subscription_status(uint16_t service_id, uint16_t instance_id,
                                                         uint16_t eventgroup_id) const {
    return impl_->get_subscription_status(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::is_ready() const {
    return impl_->is_ready();
}

EventSubscriber::Statistics EventSubscriber::get_statistics() const {
    return impl_->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::events
