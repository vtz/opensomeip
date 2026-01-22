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
#include "events/event_types.h"
#include "transport/udp_transport.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "someip/message.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace someip {
namespace events {

/**
 * @brief Event Subscriber implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_730
 * @satisfies feat_req_someip_731
 */
class EventSubscriberImpl : public transport::ITransportListener {
public:
    EventSubscriberImpl(uint16_t client_id)
        : client_id_(client_id),
          transport_(std::make_shared<transport::UdpTransport>(
              transport::Endpoint("127.0.0.1", 0))),
          running_(false) {

        transport_->set_listener(this);
    }

    ~EventSubscriberImpl() {
        shutdown();
    }

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
        std::scoped_lock subs_lock(subscriptions_mutex_);
        subscriptions_.clear();

        transport_->stop();
    }

    bool subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                            EventNotificationCallback notification_callback,
                            SubscriptionStatusCallback status_callback,
                            const std::vector<EventFilter>& filters) {

        if (!running_) {
            return false;
        }

        // Create subscription info
        EventSubscription subscription(service_id, instance_id, 0, eventgroup_id);
        subscription.state = SubscriptionState::REQUESTED;

        SubscriptionInfo sub_info;
        sub_info.subscription = subscription;
        sub_info.notification_callback = notification_callback;
        sub_info.status_callback = status_callback;
        sub_info.filters = filters;

        // Store subscription
        std::scoped_lock subs_lock(subscriptions_mutex_);
        std::string key = make_subscription_key(service_id, instance_id, eventgroup_id);
        subscriptions_[key] = sub_info;

        // Send subscription request via RPC (simplified - in real implementation,
        // this would use SD to find the service endpoint and send subscription)
        // For now, we'll assume the service is at a known endpoint
        transport::Endpoint service_endpoint("127.0.0.1", 30500);  // TODO: Get from SD

        // Create subscription message (simplified)
        MessageId msg_id(service_id, 0x0001);  // Method ID for subscription
        Message subscription_msg(msg_id, RequestId(client_id_, 0x0001),
                                MessageType::REQUEST, ReturnCode::E_OK);

        // Add subscription data to payload
        std::vector<uint8_t> payload;
        payload.push_back((eventgroup_id >> 8) & 0xFF);
        payload.push_back(eventgroup_id & 0xFF);
        subscription_msg.set_payload(payload);

        Result send_result = transport_->send_message(subscription_msg, service_endpoint);
        bool success = (send_result == Result::SUCCESS);
        if (!success) {
            subscriptions_.erase(key);
        }
        return success;
    }

    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
        if (!running_) {
            return false;
        }

        std::scoped_lock subs_lock(subscriptions_mutex_);
        std::string key = make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return false;
        }

        // Send unsubscription request
        transport::Endpoint service_endpoint("127.0.0.1", 30500);  // TODO: Get from SD

        MessageId msg_id(service_id, 0x0002);  // Method ID for unsubscription
        Message unsubscription_msg(msg_id, RequestId(client_id_, 0x0002),
                                  MessageType::REQUEST, ReturnCode::E_OK);

        // Add unsubscription data to payload
        std::vector<uint8_t> payload;
        payload.push_back((eventgroup_id >> 8) & 0xFF);
        payload.push_back(eventgroup_id & 0xFF);
        unsubscription_msg.set_payload(payload);

        Result result = transport_->send_message(unsubscription_msg, service_endpoint);
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

        // Store callback for field response
        std::scoped_lock field_lock(field_requests_mutex_);
        std::string key = make_field_key(service_id, instance_id, event_id);
        field_requests_[key] = callback;

        // Send field request
        transport::Endpoint service_endpoint("127.0.0.1", 30500);  // TODO: Get from SD

        MessageId msg_id(service_id, 0x0003);  // Method ID for field request
        Message field_msg(msg_id, RequestId(client_id_, 0x0003),
                         MessageType::REQUEST, ReturnCode::E_OK);

        // Add field ID to payload
        std::vector<uint8_t> payload;
        payload.push_back((event_id >> 8) & 0xFF);
        payload.push_back(event_id & 0xFF);
        field_msg.set_payload(payload);

        return transport_->send_message(field_msg, service_endpoint) == Result::SUCCESS;
    }

    bool set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                         const std::vector<EventFilter>& filters) {

        std::scoped_lock subs_lock(subscriptions_mutex_);
        std::string key = make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return false;
        }

        it->second.filters = filters;
        return true;
    }

    std::vector<EventSubscription> get_active_subscriptions() const {
        std::scoped_lock subs_lock(subscriptions_mutex_);
        std::vector<EventSubscription> result;

        for (const auto& pair : subscriptions_) {
            result.push_back(pair.second.subscription);
        }

        return result;
    }

    SubscriptionState get_subscription_status(uint16_t service_id, uint16_t instance_id,
                                            uint16_t eventgroup_id) const {

        std::scoped_lock subs_lock(subscriptions_mutex_);
        std::string key = make_subscription_key(service_id, instance_id, eventgroup_id);

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

private:
    struct SubscriptionInfo {
        EventSubscription subscription;
        EventNotificationCallback notification_callback;
        SubscriptionStatusCallback status_callback;
        std::vector<EventFilter> filters;
    };

    std::string make_subscription_key(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) const {
        return std::to_string(service_id) + ":" + std::to_string(instance_id) + ":" + std::to_string(eventgroup_id);
    }

    std::string make_field_key(uint16_t service_id, uint16_t instance_id, uint16_t event_id) {
        return std::to_string(service_id) + ":" + std::to_string(instance_id) + ":" + std::to_string(event_id);
    }

    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override {
        // Check if this is an event notification
        if (message->get_message_type() != MessageType::NOTIFICATION) {
            return;
        }

        // Check if this is for one of our subscriptions
        std::scoped_lock subs_lock(subscriptions_mutex_);

        uint16_t service_id = message->get_service_id();
        uint16_t event_id = message->get_method_id();  // Event ID is in method ID field for notifications

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
        std::scoped_lock field_lock(field_requests_mutex_);
        std::string field_key = make_field_key(service_id, 0, event_id);  // Simplified

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

    void on_connection_lost(const transport::Endpoint& endpoint) override {
        // Handle service disconnection
        std::scoped_lock subs_lock(subscriptions_mutex_);

        for (auto& sub_pair : subscriptions_) {
            auto& sub_info = sub_pair.second;
            if (sub_info.subscription.state == SubscriptionState::SUBSCRIBED) {
                sub_info.subscription.state = SubscriptionState::PENDING;
                // TODO: Attempt to reconnect
            }
        }
    }

    void on_connection_established(const transport::Endpoint& endpoint) override {
        // Handle service reconnection
    }

    void on_error(Result error) override {
        // Handle transport errors
    }

    uint16_t client_id_;
    std::shared_ptr<transport::UdpTransport> transport_;

    std::unordered_map<std::string, SubscriptionInfo> subscriptions_;
    mutable std::mutex subscriptions_mutex_;

    std::unordered_map<std::string, EventNotificationCallback> field_requests_;
    mutable std::mutex field_requests_mutex_;

    std::atomic<bool> running_;
};

// EventSubscriber implementation
EventSubscriber::EventSubscriber(uint16_t client_id)
    : impl_(std::make_unique<EventSubscriberImpl>(client_id)) {
}

EventSubscriber::~EventSubscriber() = default;

bool EventSubscriber::initialize() {
    return impl_->initialize();
}

void EventSubscriber::shutdown() {
    impl_->shutdown();
}

bool EventSubscriber::subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                         EventNotificationCallback notification_callback,
                                         SubscriptionStatusCallback status_callback,
                                         const std::vector<EventFilter>& filters) {
    return impl_->subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                                     notification_callback, status_callback, filters);
}

bool EventSubscriber::unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
    return impl_->unsubscribe_eventgroup(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::request_field(uint16_t service_id, uint16_t instance_id, uint16_t event_id,
                                   EventNotificationCallback callback) {
    return impl_->request_field(service_id, instance_id, event_id, callback);
}

bool EventSubscriber::set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                      const std::vector<EventFilter>& filters) {
    return impl_->set_event_filters(service_id, instance_id, eventgroup_id, filters);
}

std::vector<EventSubscription> EventSubscriber::get_active_subscriptions() const {
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

} // namespace events
} // namespace someip
