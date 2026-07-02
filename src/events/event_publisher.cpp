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

#ifdef SOMEIP_STATIC_ALLOC
#include <new>
#endif

#include "common/result.h"
#include "events/event_types.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::UnorderedMap via containers dispatch header
#include "platform/containers.h"
#include "platform/thread.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"
#include "transport/udp_transport.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

namespace someip::events {

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex / platform::this_thread from platform/thread.h (IWYU false positives in impl).

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
          // TODO: Replace with pool-based or aligned-storage transport for full no-heap compliance
          transport_(std::make_shared<transport::UdpTransport>(
              transport::Endpoint("0.0.0.0", 0))),
          next_session_id_(1), running_(false) {

        transport_->set_listener(this);
    }

    ~EventPublisherImpl() override
    {
        shutdown();
    }

    EventPublisherImpl(const EventPublisherImpl&) = delete;
    EventPublisherImpl& operator=(const EventPublisherImpl&) = delete;
    EventPublisherImpl(EventPublisherImpl&&) = delete;
    EventPublisherImpl& operator=(EventPublisherImpl&&) = delete;

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

        {
            platform::ScopedLock const events_lock(events_mutex_);
            registered_events_.clear();
        }
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            subscriptions_.clear();
        }

        transport_->stop();
    }

    bool register_event(const EventConfig& config) {
        platform::ScopedLock const events_lock(events_mutex_);

        // Check if already registered
        bool const already_exists = registered_events_.count(config.event_id) > 0;
        if (!already_exists) {
            registered_events_[config.event_id] = config;
        }
        return !already_exists;
    }

    bool unregister_event(uint16_t event_id) {
        platform::ScopedLock const events_lock(events_mutex_);
        return registered_events_.erase(event_id) > 0;
    }

    bool update_event_config(uint16_t event_id, const EventConfig& config) {
        platform::ScopedLock const events_lock(events_mutex_);

        auto it = registered_events_.find(event_id);
        if (it == registered_events_.end()) {
            return false;
        }

        it->second = config;
        return true;
    }

    /** @implements REQ_MSG_110, REQ_MSG_110_E01, REQ_MSG_119, REQ_MSG_121A, REQ_MSG_121B, REQ_MSG_121C, REQ_MSG_121_E01, REQ_MSG_121_E02, REQ_MSG_141 */
    bool publish_event(uint16_t event_id, const platform::ByteBuffer& data) {
        if (!running_) {
            return false;
        }

        uint16_t eventgroup_id = 0;
        {
            platform::ScopedLock const events_lock(events_mutex_);
            auto event_it = registered_events_.find(event_id);
            if (event_it == registered_events_.end()) {
                return false;
            }
            eventgroup_id = event_it->second.eventgroup_id;
        }

        EventNotification notification(service_id_, instance_id_, event_id);
        notification.event_data = data;
        notification.session_id = next_session_id_++;

        platform::Vector<transport::Endpoint> targets;
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            auto sub_it = subscriptions_.find(eventgroup_id);
            if (sub_it != subscriptions_.end()) {
                for (const auto& client_info : sub_it->second) {
                    if (!client_info.is_expired()) {
                        targets.push_back(client_info.endpoint);
                    }
                }
            }
        }

        for (const auto& ep : targets) {
            send_event_notification(notification, ep);
        }

        return true;
    }

    bool publish_field(uint16_t event_id, const platform::ByteBuffer& data) {
        // Fields are published immediately like events
        return publish_event(event_id, data);
    }

    void set_default_client_endpoint(const platform::String<>& address, uint16_t port) {
        platform::ScopedLock const lock(subscriptions_mutex_);
        default_client_address_ = address;
        default_client_port_ = port;
    }

    /** @implements REQ_MSG_124, REQ_MSG_124_E01, REQ_MSG_125, REQ_MSG_125_E01, REQ_MSG_126 */
    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                           const platform::Vector<EventFilter>& filters) {
        return handle_subscription(eventgroup_id, client_id, TTL_INFINITE, filters);
    }

    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                           uint32_t ttl_seconds,
                           const platform::Vector<EventFilter>& filters) {
        platform::ScopedLock const lock(subscriptions_mutex_);
        if (default_client_port_ == 0) {
            return false;
        }
        return handle_subscription_locked(eventgroup_id, client_id,
                                          transport::Endpoint(default_client_address_, default_client_port_),
                                          ttl_seconds, filters);
    }

    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                           const transport::Endpoint& client_endpoint,
                           const platform::Vector<EventFilter>& filters) {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        return handle_subscription_locked(eventgroup_id, client_id, client_endpoint,
                                          TTL_INFINITE, filters);
    }

    bool handle_subscription_locked(uint16_t eventgroup_id, uint16_t client_id,
                                    const transport::Endpoint& client_endpoint,
                                    uint32_t ttl_seconds,
                                    const platform::Vector<EventFilter>& filters) {

        if (ttl_seconds == 0) {
            auto sub_it = subscriptions_.find(eventgroup_id);
            if (sub_it == subscriptions_.end()) {
                return false;
            }
            auto& clients = sub_it->second;
            auto new_end = std::remove_if(clients.begin(), clients.end(),
                [client_id](const ClientInfo& info) {
                    return info.client_id == client_id;
                });
            bool const found = (new_end != clients.end());
            clients.erase(new_end, clients.end());
            return found;
        }

        ClientInfo client_info;
        client_info.client_id = client_id;
        client_info.endpoint = client_endpoint;
        client_info.filters = filters;
        client_info.ttl_seconds = ttl_seconds;
        client_info.subscribed_at = std::chrono::steady_clock::now();

        auto& clients = subscriptions_[eventgroup_id];
        auto it = std::find_if(clients.begin(), clients.end(),
            [client_id](const ClientInfo& info) {
                return info.client_id == client_id;
            });

        if (it == clients.end()) {
            clients.push_back(client_info);
        } else {
            *it = client_info;
        }

        return true;
    }

    bool handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id) {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

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

    platform::Vector<uint16_t> get_registered_events() const {
        platform::ScopedLock const events_lock(events_mutex_);
        platform::Vector<uint16_t> events;
        events.reserve(registered_events_.size());

        for (const auto& pair : registered_events_) {
            events.push_back(pair.first);
        }

        return events;
    }

    platform::Vector<uint16_t> get_subscriptions(uint16_t eventgroup_id) const {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

        auto it = subscriptions_.find(eventgroup_id);
        if (it == subscriptions_.end()) {
            return {};
        }

        platform::Vector<uint16_t> client_ids;
        for (const auto& client : it->second) {
            if (!client.is_expired()) {
                client_ids.push_back(client.client_id);
            }
        }

        return client_ids;
    }

    size_t cleanup_expired_subscriptions() {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        size_t removed = 0;

        for (auto& sub_pair : subscriptions_) {
            auto& clients = sub_pair.second;
            auto new_end = std::remove_if(clients.begin(), clients.end(),
                [](const ClientInfo& info) { return info.is_expired(); });
            removed += static_cast<size_t>(std::distance(new_end, clients.end()));
            clients.erase(new_end, clients.end());
        }

        return removed;
    }

    bool is_ready() const {
        return running_ && transport_->is_connected();
    }

    EventPublisher::Statistics get_statistics() const {
        // TODO: Implement statistics tracking
        return EventPublisher::Statistics{};
    }

private:
    static constexpr uint32_t TTL_INFINITE = 0xFFFFFF;

    struct ClientInfo {
        uint16_t client_id{};
        transport::Endpoint endpoint;
        platform::Vector<EventFilter> filters;
        uint32_t ttl_seconds{TTL_INFINITE};
        std::chrono::steady_clock::time_point subscribed_at{std::chrono::steady_clock::now()};

        bool is_expired() const {
            if (ttl_seconds >= TTL_INFINITE) {
                return false;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - subscribed_at);
            return elapsed.count() >= static_cast<long>(ttl_seconds);
        }
    };

    void start_publish_timer() {
        if (publish_timer_thread_ && publish_timer_thread_->joinable()) {
            return;
        }

        publish_timer_thread_.emplace([this]() {
            uint32_t tick_count = 0;
            while (running_) {
                platform::this_thread::sleep_for(std::chrono::milliseconds(100));  // 100ms check

                if (!running_) {
                    break;
                }

                publish_cyclic_events();

                if (++tick_count >= 10) {
                    tick_count = 0;
                    cleanup_expired_subscriptions();
                }
            }
        });
    }

    void stop_publish_timer() {
        if (publish_timer_thread_ && publish_timer_thread_->joinable()) {
            publish_timer_thread_->join();
        }
    }

    void publish_cyclic_events() {
        platform::Vector<uint16_t> events_to_publish;
        {
            platform::ScopedLock const events_lock(events_mutex_);
            auto now = std::chrono::steady_clock::now();

            for (auto& event_pair : registered_events_) {
                const auto& config = event_pair.second;

                if (config.notification_type == NotificationType::PERIODIC &&
                    config.cycle_time.count() > 0) {

                    auto const time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_publish_times_[config.event_id]);

                    if (time_since_last >= config.cycle_time) {
                        events_to_publish.push_back(config.event_id);
                        last_publish_times_[config.event_id] = now;
                    }
                }
            }
        }

        for (const uint16_t eid : events_to_publish) {
            publish_event(eid, {});
        }
    }

    void send_event_notification(const EventNotification& notification,
                               const transport::Endpoint& client_endpoint) {

        // Create SOME/IP message for event notification
        MessageId const msg_id(service_id_, notification.event_id);
        Message someip_message(msg_id,
                               RequestId(notification.client_id, notification.session_id),
                               MessageType::NOTIFICATION, ReturnCode::E_OK);
        someip_message.set_payload(notification.event_data);

        Result const result = transport_->send_message(someip_message, client_endpoint);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }
    }

    void on_message_received(MessagePtr /*message*/, const transport::Endpoint& /*sender*/) override {
        // Handle subscription/unsubscription messages
        // This would typically come from SD or direct subscription messages
    }

    void on_connection_lost(const transport::Endpoint& endpoint) override {
        // Handle client disconnection
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

        for (auto& sub_pair : subscriptions_) {
            auto& clients = sub_pair.second;
            auto it = std::remove_if(clients.begin(), clients.end(),
                [&endpoint](const ClientInfo& info) {
                    return info.endpoint == endpoint;
                });
            clients.erase(it, clients.end());
        }
    }

    void on_connection_established(const transport::Endpoint& /*endpoint*/) override {
        // Handle new client connections
    }

    void on_error(Result /*error*/) override {
        // Handle transport errors
    }

    uint16_t service_id_;
    uint16_t instance_id_;
    platform::String<> default_client_address_{"0.0.0.0"};
    uint16_t default_client_port_{0};
    std::shared_ptr<transport::UdpTransport> transport_;

    platform::UnorderedMap<uint16_t, EventConfig, 32> registered_events_;
    mutable platform::Mutex events_mutex_;

    platform::UnorderedMap<uint16_t, platform::Vector<ClientInfo>, 32> subscriptions_;
    mutable platform::Mutex subscriptions_mutex_;

    platform::UnorderedMap<uint16_t, std::chrono::steady_clock::time_point, 32> last_publish_times_;
    std::optional<platform::Thread> publish_timer_thread_;
    std::atomic<uint16_t> next_session_id_;
    std::atomic<bool> running_;
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(EventPublisherImpl) <= SOMEIP_PIMPL_EVENTPUB_SIZE,
              "EventPublisherImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_EVENTPUB_SIZE");
#endif

// EventPublisher implementation
EventPublisher::EventPublisher(uint16_t service_id, uint16_t instance_id)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) EventPublisherImpl(service_id, instance_id);
}
#else
    : impl_(std::make_unique<EventPublisherImpl>(service_id, instance_id)) {
}
#endif

EventPublisher::~EventPublisher() {
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~EventPublisherImpl();
#endif
}

bool EventPublisher::initialize() {
    return impl()->initialize();
}

void EventPublisher::shutdown() {
    impl()->shutdown();
}

bool EventPublisher::register_event(const EventConfig& config) {
    return impl()->register_event(config);
}

bool EventPublisher::unregister_event(uint16_t event_id) {
    return impl()->unregister_event(event_id);
}

bool EventPublisher::update_event_config(uint16_t event_id, const EventConfig& config) {
    return impl()->update_event_config(event_id, config);
}

bool EventPublisher::publish_event(uint16_t event_id, const platform::ByteBuffer& data) {
    return impl()->publish_event(event_id, data);
}

bool EventPublisher::publish_field(uint16_t event_id, const platform::ByteBuffer& data) {
    return impl()->publish_field(event_id, data);
}

void EventPublisher::set_default_client_endpoint(const platform::String<>& address, uint16_t port) {
    impl()->set_default_client_endpoint(address, port);
}

bool EventPublisher::handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                                       const platform::Vector<EventFilter>& filters) {
    return impl()->handle_subscription(eventgroup_id, client_id, filters);
}

bool EventPublisher::handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                                       uint32_t ttl_seconds,
                                       const platform::Vector<EventFilter>& filters) {
    return impl()->handle_subscription(eventgroup_id, client_id, ttl_seconds, filters);
}

bool EventPublisher::handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id) {
    return impl()->handle_unsubscription(eventgroup_id, client_id);
}

size_t EventPublisher::cleanup_expired_subscriptions() {
    return impl()->cleanup_expired_subscriptions();
}

platform::Vector<uint16_t> EventPublisher::get_registered_events() const {
    return impl()->get_registered_events();
}

platform::Vector<uint16_t> EventPublisher::get_subscriptions(uint16_t eventgroup_id) const {
    return impl()->get_subscriptions(eventgroup_id);
}

bool EventPublisher::is_ready() const {
    return impl()->is_ready();
}

EventPublisher::Statistics EventPublisher::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::events
