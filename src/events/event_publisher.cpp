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

#include "events/event_publisher.h"
#include "events/event_types.h"
#include "transport/udp_transport.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "someip/message.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>

namespace someip {
namespace events {

/**
 * @brief Event Publisher implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_720
 * @satisfies feat_req_someip_721
 */
class EventPublisherImpl : public transport::ITransportListener {
public:
    EventPublisherImpl(uint16_t service_id, uint16_t instance_id)
        : service_id_(service_id), instance_id_(instance_id),
          transport_(std::make_shared<transport::UdpTransport>(
              transport::Endpoint("127.0.0.1", 0))),
          running_(false), next_session_id_(1) {

        transport_->set_listener(this);
    }

    ~EventPublisherImpl() {
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
        start_publish_timer();

        return true;
    }

    void shutdown() {
        if (!running_) {
            return;
        }

        running_ = false;
        stop_publish_timer();

        // Clear all subscriptions and events
        std::scoped_lock subs_lock(subscriptions_mutex_);
        subscriptions_.clear();

        std::scoped_lock events_lock(events_mutex_);
        registered_events_.clear();

        transport_->stop();
    }

    bool register_event(const EventConfig& config) {
        std::scoped_lock events_lock(events_mutex_);

        // Check if already registered
        bool already_exists = registered_events_.count(config.event_id) > 0;
        if (!already_exists) {
            registered_events_[config.event_id] = config;
        }
        return !already_exists;
    }

    bool unregister_event(uint16_t event_id) {
        std::scoped_lock events_lock(events_mutex_);
        return registered_events_.erase(event_id) > 0;
    }

    bool update_event_config(uint16_t event_id, const EventConfig& config) {
        std::scoped_lock events_lock(events_mutex_);

        auto it = registered_events_.find(event_id);
        if (it == registered_events_.end()) {
            return false;
        }

        it->second = config;
        return true;
    }

    bool publish_event(uint16_t event_id, const std::vector<uint8_t>& data) {
        if (!running_) {
            return false;
        }

        std::scoped_lock events_lock(events_mutex_);
        auto event_it = registered_events_.find(event_id);
        if (event_it == registered_events_.end()) {
            return false;
        }

        // Create event notification
        EventNotification notification(service_id_, instance_id_, event_id);
        notification.event_data = data;
        notification.session_id = next_session_id_++;

        // Send to all subscribed clients for this event's eventgroup
        std::scoped_lock subs_lock(subscriptions_mutex_);
        auto eventgroup_id = event_it->second.eventgroup_id;

        auto sub_it = subscriptions_.find(eventgroup_id);
        if (sub_it != subscriptions_.end()) {
            for (const auto& client_info : sub_it->second) {
                send_event_notification(notification, client_info.endpoint);
            }
        }

        return true;
    }

    bool publish_field(uint16_t event_id, const std::vector<uint8_t>& data) {
        // Fields are published immediately like events
        return publish_event(event_id, data);
    }

    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                           const std::vector<EventFilter>& filters) {

        std::scoped_lock subs_lock(subscriptions_mutex_);

        // Create client info (simplified - using localhost for demo)
        ClientInfo client_info;
        client_info.client_id = client_id;
        client_info.endpoint = transport::Endpoint("127.0.0.1", 30500);  // TODO: Get from SD
        client_info.filters = filters;

        auto& clients = subscriptions_[eventgroup_id];
        auto it = std::find_if(clients.begin(), clients.end(),
            [client_id](const ClientInfo& info) {
                return info.client_id == client_id;
            });

        if (it == clients.end()) {
            clients.push_back(client_info);
        } else {
            *it = client_info;  // Update existing
        }

        return true;
    }

    bool handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id) {
        std::scoped_lock subs_lock(subscriptions_mutex_);

        auto sub_it = subscriptions_.find(eventgroup_id);
        if (sub_it == subscriptions_.end()) {
            return false;
        }

        auto& clients = sub_it->second;
        auto it = std::remove_if(clients.begin(), clients.end(),
            [client_id](const ClientInfo& info) {
                return info.client_id == client_id;
            });

        clients.erase(it, clients.end());
        return true;
    }

    std::vector<uint16_t> get_registered_events() const {
        std::scoped_lock events_lock(events_mutex_);
        std::vector<uint16_t> events;

        for (const auto& pair : registered_events_) {
            events.push_back(pair.first);
        }

        return events;
    }

    std::vector<uint16_t> get_subscriptions(uint16_t eventgroup_id) const {
        std::scoped_lock subs_lock(subscriptions_mutex_);

        auto it = subscriptions_.find(eventgroup_id);
        if (it == subscriptions_.end()) {
            return {};
        }

        std::vector<uint16_t> client_ids;
        for (const auto& client : it->second) {
            client_ids.push_back(client.client_id);
        }

        return client_ids;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    EventPublisher::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return EventPublisher::Statistics{};
    }

private:
    struct ClientInfo {
        uint16_t client_id;
        transport::Endpoint endpoint;
        std::vector<EventFilter> filters;
    };

    void start_publish_timer() {
        if (publish_timer_thread_.joinable()) {
            return;
        }

        publish_timer_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 100ms check

                if (!running_) {
                    break;
                }

                publish_cyclic_events();
            }
        });
    }

    void stop_publish_timer() {
        if (publish_timer_thread_.joinable()) {
            publish_timer_thread_.join();
        }
    }

    void publish_cyclic_events() {
        std::scoped_lock events_lock(events_mutex_);
        auto now = std::chrono::steady_clock::now();

        for (auto& event_pair : registered_events_) {
            const auto& config = event_pair.second;

            if (config.notification_type == NotificationType::PERIODIC &&
                config.cycle_time.count() > 0) {

                // Check if it's time to publish
                auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_publish_times_[config.event_id]);

                if (time_since_last >= config.cycle_time) {
                    // Publish with empty data (or default data)
                    publish_event(config.event_id, {});
                    last_publish_times_[config.event_id] = now;
                }
            }
        }
    }

    void send_event_notification(const EventNotification& notification,
                               const transport::Endpoint& client_endpoint) {

        // Create SOME/IP message for event notification
        MessageId msg_id(service_id_, notification.event_id);
        Message someip_message(msg_id, RequestId(notification.client_id, notification.session_id),
                              MessageType::NOTIFICATION, ReturnCode::E_OK);
        someip_message.set_payload(notification.event_data);

        Result result = transport_->send_message(someip_message, client_endpoint);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }
    }

    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override {
        // Handle subscription/unsubscription messages
        // This would typically come from SD or direct subscription messages
    }

    void on_connection_lost(const transport::Endpoint& endpoint) override {
        // Handle client disconnection
        std::scoped_lock subs_lock(subscriptions_mutex_);

        for (auto& sub_pair : subscriptions_) {
            auto& clients = sub_pair.second;
            auto it = std::remove_if(clients.begin(), clients.end(),
                [&endpoint](const ClientInfo& info) {
                    return info.endpoint == endpoint;
                });
            clients.erase(it, clients.end());
        }
    }

    void on_connection_established(const transport::Endpoint& endpoint) override {
        // Handle new client connections
    }

    void on_error(Result error) override {
        // Handle transport errors
    }

    uint16_t service_id_;
    uint16_t instance_id_;
    std::shared_ptr<transport::UdpTransport> transport_;

    std::unordered_map<uint16_t, EventConfig> registered_events_;
    mutable std::mutex events_mutex_;

    std::unordered_map<uint16_t, std::vector<ClientInfo>> subscriptions_;
    mutable std::mutex subscriptions_mutex_;

    std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> last_publish_times_;
    std::thread publish_timer_thread_;
    std::atomic<uint16_t> next_session_id_;
    std::atomic<bool> running_;
};

// EventPublisher implementation
EventPublisher::EventPublisher(uint16_t service_id, uint16_t instance_id)
    : impl_(std::make_unique<EventPublisherImpl>(service_id, instance_id)) {
}

EventPublisher::~EventPublisher() = default;

bool EventPublisher::initialize() {
    return impl_->initialize();
}

void EventPublisher::shutdown() {
    impl_->shutdown();
}

bool EventPublisher::register_event(const EventConfig& config) {
    return impl_->register_event(config);
}

bool EventPublisher::unregister_event(uint16_t event_id) {
    return impl_->unregister_event(event_id);
}

bool EventPublisher::update_event_config(uint16_t event_id, const EventConfig& config) {
    return impl_->update_event_config(event_id, config);
}

bool EventPublisher::publish_event(uint16_t event_id, const std::vector<uint8_t>& data) {
    return impl_->publish_event(event_id, data);
}

bool EventPublisher::publish_field(uint16_t event_id, const std::vector<uint8_t>& data) {
    return impl_->publish_field(event_id, data);
}

bool EventPublisher::handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                                       const std::vector<EventFilter>& filters) {
    return impl_->handle_subscription(eventgroup_id, client_id, filters);
}

bool EventPublisher::handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id) {
    return impl_->handle_unsubscription(eventgroup_id, client_id);
}

std::vector<uint16_t> EventPublisher::get_registered_events() const {
    return impl_->get_registered_events();
}

std::vector<uint16_t> EventPublisher::get_subscriptions(uint16_t eventgroup_id) const {
    return impl_->get_subscriptions(eventgroup_id);
}

bool EventPublisher::is_ready() const {
    return impl_->is_ready();
}

EventPublisher::Statistics EventPublisher::get_statistics() const {
    return impl_->get_statistics();
}

} // namespace events
} // namespace someip
