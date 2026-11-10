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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <new>  // NOLINT(misc-include-cleaner) - static allocation placement new
#include <optional>
#include <unordered_map>
#include <utility>

#include "../transport/transport_session.h"
#include "common/result.h"
#include "events/event_types.h"
#include "platform/containers.h"  // NOLINT(misc-include-cleaner) - PAL dispatch
#include "platform/thread.h"
#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/transport.h"

namespace someip::events {

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex / platform::this_thread from
// platform/thread.h (IWYU false positives in impl).

/**
 * @brief Event Publisher implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_720
 * @satisfies feat_req_someip_721
 */
class EventPublisherImpl : public transport::ITransportListener {
   public:
    template <typename Transport>
    EventPublisherImpl(uint16_t service_id, uint16_t instance_id, Transport&& transport)
        : service_id_(service_id),
          instance_id_(instance_id),
          transport_session_(std::forward<Transport>(transport)),
          transport_(transport_session_.get()),
          next_session_id_(1),
          running_(false)
    {
    }

    ~EventPublisherImpl() override
    {
#ifdef __cpp_exceptions
        try {
            shutdown();
        }
        catch (...) {  // NOLINT(bugprone-empty-catch) destructor must not throw
        }
#else
        shutdown();
#endif
    }

    EventPublisherImpl(const EventPublisherImpl&) = delete;
    EventPublisherImpl& operator=(const EventPublisherImpl&) = delete;
    EventPublisherImpl(EventPublisherImpl&&) = delete;
    EventPublisherImpl& operator=(EventPublisherImpl&&) = delete;

    Result get_transport_result() const
    {
        return transport_session_.result();
    }

    bool initialize()
    {
        if (running_) {
            return true;
        }

        if (transport_session_.start(*this) != Result::SUCCESS) {
            return false;
        }

        running_ = true;
        start_publish_timer();

        return true;
    }

    void shutdown()
    {
        if (!running_) {
            transport_session_.stop();
            return;
        }

        running_ = false;
        stop_publish_timer();
        transport_session_.stop();

        {
            platform::ScopedLock const events_lock(events_mutex_);
            registered_events_.clear();
            field_values_.clear();
            fields_.clear();
            getter_index_.clear();
            setter_index_.clear();
            field_setter_ = FieldSetter{};
        }
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            subscriptions_.clear();
        }
    }

    /** @implements REQ_MSG_126, REQ_MSG_125_E01 */
    bool register_event(const EventConfig& config)
    {
        platform::ScopedLock const events_lock(events_mutex_);
        return register_event_locked(config);
    }

    bool unregister_event(uint16_t event_id)
    {
        platform::ScopedLock const events_lock(events_mutex_);
        bool removed = false;
        FieldBinding binding;
        if (find_field_locked(event_id, binding)) {
            if (binding.has_notifier) {
                registered_events_.erase(binding.notifier_event_id);
            }
            field_values_.erase(binding.key);
            unbind_field_locked(binding.key);
            removed = true;
        }
        if (registered_events_.erase(event_id) > 0) {
            field_values_.erase(event_id);
            removed = true;
        }
        return removed;
    }

    bool update_event_config(uint16_t event_id, const EventConfig& config)
    {
        platform::ScopedLock const events_lock(events_mutex_);
        const auto it = registered_events_.find(event_id);
        if (it == registered_events_.end()) {
            return false;
        }
        if (config.is_field && !field_has_accessor(config)) {
            return false;
        }
        if (!config.is_field) {
            FieldBinding binding;
            if (find_field_locked(event_id, binding) && binding.key == event_id) {
                field_values_.erase(binding.key);
                unbind_field_locked(binding.key);
            }
            it->second = config;
            return true;
        }
        if (config.has_notifier && config.event_id != event_id) {
            return false;
        }
        return register_event_locked(config);
    }

    /** @implements REQ_MSG_110, REQ_MSG_110_E01, REQ_MSG_119, REQ_MSG_121A */
    /** @implements REQ_MSG_121B, REQ_MSG_121C, REQ_MSG_121_E01, REQ_MSG_121_E02, REQ_MSG_141 */
    bool publish_event(uint16_t event_id, const platform::ByteBuffer& data)
    {
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
            if (event_it->second.is_field) {
                if (field_values_.size() >= field_values_.max_size() &&
                    field_values_.find(event_id) == field_values_.end()) {
                    return false;
                }
                field_values_[event_id] = data;
            }
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

    /** @implements REQ_MSG_125 */
    bool publish_field(uint16_t event_id, const platform::ByteBuffer& data)
    {
        bool found = false;
        bool notify = false;
        uint16_t notifier_id = 0;
        {
            platform::ScopedLock const events_lock(events_mutex_);
            FieldBinding binding;
            if (find_field_locked(event_id, binding)) {
                if (field_values_.size() >= field_values_.max_size() &&
                    field_values_.find(binding.key) == field_values_.end()) {
                    return false;
                }
                field_values_[binding.key] = data;
                found = true;
                notify = binding.has_notifier;
                notifier_id = binding.notifier_event_id;
            }
        }
        if (found) {
            if (!notify) {
                return true;
            }
            return publish_event(notifier_id, data);
        }
        return publish_event(event_id, data);
    }

    transport::Endpoint get_local_endpoint() const
    {
        return transport_.get_local_endpoint();
    }

    void set_field_setter(FieldSetter setter)
    {
        platform::ScopedLock const events_lock(events_mutex_);
        field_setter_ = std::move(setter);
    }

    void set_default_client_endpoint(const platform::String<>& address, uint16_t port)
    {
        platform::ScopedLock const lock(subscriptions_mutex_);
        default_client_address_ = address;
        default_client_port_ = port;
    }

    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                             const platform::Vector<EventFilter>& filters)
    {
        return handle_subscription(eventgroup_id, client_id, TTL_INFINITE, filters);
    }

    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id, uint32_t ttl_seconds,
                             const platform::Vector<EventFilter>& filters)
    {
        bool is_new = false;
        bool ok = false;
        transport::Endpoint dest;
        {
            platform::ScopedLock const lock(subscriptions_mutex_);
            if (default_client_port_ == 0) {
                return false;
            }
            dest = transport::Endpoint(default_client_address_, default_client_port_);
            is_new = !has_subscriber_locked(eventgroup_id, client_id);
            ok = handle_subscription_locked(eventgroup_id, client_id, dest, ttl_seconds, filters);
        }
        if (ok && is_new && ttl_seconds > 0) {
            send_initial_fields(eventgroup_id, dest);
        }
        return ok;
    }

    /** @implements REQ_SD_331, REQ_MSG_125
     *  @satisfies feat_req_someipsd_1194 */
    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id, uint32_t ttl_seconds,
                             const platform::Vector<EventFilter>& filters, bool send_initial_events)
    {
        bool ok = false;
        transport::Endpoint dest;
        {
            platform::ScopedLock const lock(subscriptions_mutex_);
            if (default_client_port_ == 0) {
                return false;
            }
            dest = transport::Endpoint(default_client_address_, default_client_port_);
            ok = handle_subscription_locked(eventgroup_id, client_id, dest, ttl_seconds, filters);
        }
        if (ok && send_initial_events && ttl_seconds > 0) {
            send_initial_fields(eventgroup_id, dest);
        }
        return ok;
    }

    bool handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                             const transport::Endpoint& client_endpoint,
                             const platform::Vector<EventFilter>& filters)
    {
        bool is_new = false;
        bool ok = false;
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            is_new = !has_subscriber_locked(eventgroup_id, client_id);
            ok = handle_subscription_locked(eventgroup_id, client_id, client_endpoint, TTL_INFINITE,
                                            filters);
        }
        if (ok && is_new) {
            send_initial_fields(eventgroup_id, client_endpoint);
        }
        return ok;
    }

    bool handle_subscription_locked(uint16_t eventgroup_id, uint16_t client_id,
                                    const transport::Endpoint& client_endpoint,
                                    uint32_t ttl_seconds,
                                    const platform::Vector<EventFilter>& filters)
    {
        if (ttl_seconds == 0) {
            auto sub_it = subscriptions_.find(eventgroup_id);
            if (sub_it == subscriptions_.end()) {
                return false;
            }
            auto& clients = sub_it->second;
            auto new_end = std::remove_if(
                clients.begin(), clients.end(),
                [client_id](const ClientInfo& info) { return info.client_id == client_id; });
            bool const found = (new_end != clients.end());
            clients.erase(new_end, clients.end());
            return found;
        }

        ClientInfo client_info;
        client_info.client_id = client_id;
        client_info.endpoint = client_endpoint;
        if (filters.size() > client_info.filters.max_size()) {
            return false;
        }
        for (const auto& f : filters) {
            client_info.filters.push_back(f);
        }
        client_info.ttl_seconds = ttl_seconds;
        client_info.subscribed_at = std::chrono::steady_clock::now();

        if (subscriptions_.size() >= subscriptions_.max_size() &&
            subscriptions_.find(eventgroup_id) == subscriptions_.end()) {
            return false;
        }
        auto& clients = subscriptions_[eventgroup_id];
        auto it = std::find_if(clients.begin(), clients.end(), [client_id](const ClientInfo& info) {
            return info.client_id == client_id;
        });

        if (it == clients.end()) {
            if (clients.size() >= clients.max_size()) {
                return false;
            }
            clients.push_back(client_info);
        }
        else {
            *it = client_info;
        }

        return true;
    }

    bool has_subscriber_locked(uint16_t eventgroup_id, uint16_t client_id) const
    {
        auto sub_it = subscriptions_.find(eventgroup_id);
        if (sub_it == subscriptions_.end()) {
            return false;
        }
        const auto& clients = sub_it->second;
        return std::find_if(clients.begin(), clients.end(), [client_id](const ClientInfo& info) {
                   return info.client_id == client_id && !info.is_expired();
               }) != clients.end();
    }

    void send_initial_fields(uint16_t eventgroup_id, const transport::Endpoint& dest)
    {
        if (!running_) {
            return;
        }

        struct PendingField {
            uint16_t event_id{};
            platform::ByteBuffer payload;
        };
        platform::Vector<PendingField> pending;

        {
            platform::ScopedLock const events_lock(events_mutex_);
            for (const auto& event_pair : registered_events_) {
                const auto& config = event_pair.second;
                if (!config.is_field || config.eventgroup_id != eventgroup_id) {
                    continue;
                }
                auto val_it = field_values_.find(config.event_id);
                if (val_it == field_values_.end()) {
                    continue;
                }
                PendingField item;
                item.event_id = config.event_id;
                item.payload = val_it->second;
                pending.push_back(std::move(item));
            }
        }

        for (const auto& item : pending) {
            EventNotification notification(service_id_, instance_id_, item.event_id);
            notification.event_data = item.payload;
            notification.session_id = next_session_id_++;
            send_event_notification(notification, dest);
        }
    }

    bool handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id)
    {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

        auto sub_it = subscriptions_.find(eventgroup_id);
        if (sub_it == subscriptions_.end()) {
            return false;
        }

        auto& clients = sub_it->second;
        auto it = std::remove_if(
            clients.begin(), clients.end(),
            [client_id](const ClientInfo& info) { return info.client_id == client_id; });

        clients.erase(it, clients.end());
        return true;
    }

    platform::Vector<uint16_t> get_registered_events() const
    {
        platform::ScopedLock const events_lock(events_mutex_);
        platform::Vector<uint16_t> events;
        events.reserve(registered_events_.size());

        for (const auto& pair : registered_events_) {
            events.push_back(pair.first);
        }

        return events;
    }

    platform::Vector<uint16_t> get_subscriptions(uint16_t eventgroup_id) const
    {
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

    size_t cleanup_expired_subscriptions()
    {
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

    bool is_ready() const
    {
        return running_ && transport_.is_connected();
    }

    EventPublisher::Statistics get_statistics() const
    {
        // TODO: Implement statistics tracking
        return EventPublisher::Statistics{};
    }

    void set_message_rejection_handler(
        platform::Function<void(const transport::MessageRejectionInfo&)> handler)
    {
        platform::ScopedLock const lock(rejection_mutex_);
        rejection_handler_ = std::move(handler);
    }

   private:
    static constexpr uint32_t TTL_INFINITE = 0xFFFFFF;

    struct FieldBinding {
        uint16_t key{0};
        bool has_notifier{false};
        uint16_t notifier_event_id{0};
        bool has_getter{false};
        uint16_t getter_method_id{0};
        bool has_setter{false};
        uint16_t setter_method_id{0};
    };

    struct ClientInfo {
        uint16_t client_id{};
        transport::Endpoint endpoint;
        platform::Vector<EventFilter, 4> filters;
        uint32_t ttl_seconds{TTL_INFINITE};
        std::chrono::steady_clock::time_point subscribed_at{std::chrono::steady_clock::now()};

        bool is_expired() const
        {
            if (ttl_seconds >= TTL_INFINITE) {
                return false;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - subscribed_at);
            return elapsed.count() >= static_cast<long>(ttl_seconds);
        }
    };

    void start_publish_timer()
    {
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

    void stop_publish_timer()
    {
        if (publish_timer_thread_ && publish_timer_thread_->joinable()) {
            publish_timer_thread_->join();
        }
    }

    void publish_cyclic_events()
    {
        platform::Vector<uint16_t> events_to_publish;
        {
            platform::ScopedLock const events_lock(events_mutex_);
            auto now = std::chrono::steady_clock::now();

            for (auto& event_pair : registered_events_) {
                const auto& config = event_pair.second;

                if (config.notification_type == NotificationType::PERIODIC &&
                    config.cycle_time.count() > 0) {
                    if (last_publish_times_.size() >= last_publish_times_.max_size() &&
                        last_publish_times_.find(config.event_id) == last_publish_times_.end()) {
                        continue;
                    }
                    auto const time_since_last =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - last_publish_times_[config.event_id]);

                    if (time_since_last >= config.cycle_time) {
                        events_to_publish.push_back(config.event_id);
                        last_publish_times_[config.event_id] = now;
                    }
                }
            }
        }

        for (const uint16_t eid : events_to_publish) {
            platform::ByteBuffer payload;
            bool skip_empty_field = false;
            {
                platform::ScopedLock const events_lock(events_mutex_);
                auto event_it = registered_events_.find(eid);
                if (event_it != registered_events_.end() && event_it->second.is_field) {
                    auto val_it = field_values_.find(eid);
                    if (val_it == field_values_.end()) {
                        skip_empty_field = true;
                    }
                    else {
                        payload = val_it->second;
                    }
                }
            }
            if (!skip_empty_field) {
                publish_event(eid, payload);
            }
        }
    }

    void send_event_notification(const EventNotification& notification,
                                 const transport::Endpoint& client_endpoint)
    {
        // Create SOME/IP message for event notification
        MessageId const msg_id(service_id_, notification.event_id);
        Message someip_message(msg_id, RequestId(notification.client_id, notification.session_id),
                               MessageType::NOTIFICATION, ReturnCode::E_OK);
        someip_message.set_payload(notification.event_data);

        Result const result = transport_.send_message(someip_message, client_endpoint);
        if (result != Result::SUCCESS) {
            // Log error or handle failure
        }
    }

    void on_message_received(MessagePtr message, const transport::Endpoint& sender) override
    {
        handle_field_request(message, sender);
    }

    void on_connection_lost(const transport::Endpoint& endpoint) override
    {
        // Handle client disconnection
        platform::ScopedLock const subs_lock(subscriptions_mutex_);

        for (auto& sub_pair : subscriptions_) {
            auto& clients = sub_pair.second;
            auto it = std::remove_if(
                clients.begin(), clients.end(),
                [&endpoint](const ClientInfo& info) { return info.endpoint == endpoint; });
            clients.erase(it, clients.end());
        }
    }

    void on_connection_established(const transport::Endpoint& /*endpoint*/) override
    {
        // Handle new client connections
    }

    void on_error(Result /*error*/) override
    {
        // Handle transport errors
    }

    /** @implements REQ_TRANSPORT_026 */
    void on_message_rejected(const transport::MessageRejectionInfo& info) override
    {
        platform::Function<void(const transport::MessageRejectionInfo&)> handler;
        {
            platform::ScopedLock const lock(rejection_mutex_);
            handler = rejection_handler_;
        }
        if (handler) {
            handler(info);
        }
    }

    static bool field_has_accessor(const EventConfig& config)
    {
        return config.has_notifier || config.has_getter || config.has_setter;
    }

    static bool accessor_ids_distinct(const EventConfig& config)
    {
        if (config.has_getter && config.has_setter &&
            config.getter_method_id == config.setter_method_id) {
            return false;
        }
        if (config.has_notifier && config.has_getter &&
            config.event_id == config.getter_method_id) {
            return false;
        }
        if (config.has_notifier && config.has_setter &&
            config.event_id == config.setter_method_id) {
            return false;
        }
        return true;
    }

    static uint16_t field_storage_key(const EventConfig& config)
    {
        if (config.has_notifier) {
            return config.event_id;
        }
        if (config.has_getter) {
            return config.getter_method_id;
        }
        return config.setter_method_id;
    }

    static bool owner_ignored(bool ignore_owner, uint16_t ignore_key, uint16_t owner)
    {
        return ignore_owner && owner == ignore_key;
    }

    template <typename Map>
    static bool map_has_room(const Map& index, uint16_t id)
    {
        return index.find(id) != index.end() || index.size() < index.max_size();
    }

    bool id_in_use_locked(uint16_t id, bool ignore_owner, uint16_t ignore_key) const
    {
        const auto field_it = fields_.find(id);
        if (field_it != fields_.end() &&
            !owner_ignored(ignore_owner, ignore_key, field_it->first)) {
            return true;
        }
        const auto getter_it = getter_index_.find(id);
        if (getter_it != getter_index_.end() &&
            !owner_ignored(ignore_owner, ignore_key, getter_it->second)) {
            return true;
        }
        const auto setter_it = setter_index_.find(id);
        if (setter_it != setter_index_.end() &&
            !owner_ignored(ignore_owner, ignore_key, setter_it->second)) {
            return true;
        }
        const auto event_it = registered_events_.find(id);
        return event_it != registered_events_.end() &&
               !owner_ignored(ignore_owner, ignore_key, event_it->first);
    }

    bool accessors_available_locked(const EventConfig& config, bool ignore_owner,
                                    uint16_t ignore_key) const
    {
        if (!accessor_ids_distinct(config)) {
            return false;
        }
        if (config.has_notifier && id_in_use_locked(config.event_id, ignore_owner, ignore_key)) {
            return false;
        }
        if (config.has_getter &&
            id_in_use_locked(config.getter_method_id, ignore_owner, ignore_key)) {
            return false;
        }
        if (config.has_setter &&
            id_in_use_locked(config.setter_method_id, ignore_owner, ignore_key)) {
            return false;
        }
        return true;
    }

    void unbind_field_locked(uint16_t key)
    {
        const auto it = fields_.find(key);
        if (it == fields_.end()) {
            return;
        }
        const FieldBinding binding = it->second;
        if (binding.has_getter) {
            getter_index_.erase(binding.getter_method_id);
        }
        if (binding.has_setter) {
            setter_index_.erase(binding.setter_method_id);
        }
        fields_.erase(it);
    }

    bool bind_field_locked(const EventConfig& config)
    {
        const uint16_t key = field_storage_key(config);
        if (fields_.size() >= fields_.max_size() && fields_.find(key) == fields_.end()) {
            return false;
        }
        if (config.has_getter && !map_has_room(getter_index_, config.getter_method_id)) {
            return false;
        }
        if (config.has_setter && !map_has_room(setter_index_, config.setter_method_id)) {
            return false;
        }
        FieldBinding binding;
        binding.key = key;
        binding.has_notifier = config.has_notifier;
        binding.notifier_event_id = config.event_id;
        binding.has_getter = config.has_getter;
        binding.getter_method_id = config.getter_method_id;
        binding.has_setter = config.has_setter;
        binding.setter_method_id = config.setter_method_id;
        fields_[key] = binding;
        if (config.has_getter) {
            getter_index_[config.getter_method_id] = key;
        }
        if (config.has_setter) {
            setter_index_[config.setter_method_id] = key;
        }
        return true;
    }

    bool find_field_locked(uint16_t id, FieldBinding& out) const
    {
        const auto by_key = fields_.find(id);
        if (by_key != fields_.end()) {
            out = by_key->second;
            return true;
        }
        const auto getter_it = getter_index_.find(id);
        if (getter_it != getter_index_.end()) {
            const auto field_it = fields_.find(getter_it->second);
            if (field_it != fields_.end()) {
                out = field_it->second;
                return true;
            }
        }
        const auto setter_it = setter_index_.find(id);
        if (setter_it != setter_index_.end()) {
            const auto field_it = fields_.find(setter_it->second);
            if (field_it != fields_.end()) {
                out = field_it->second;
                return true;
            }
        }
        return false;
    }

    void restore_field_locked(bool has_binding, const FieldBinding& binding, bool has_value,
                              const platform::ByteBuffer& value, bool has_event,
                              const EventConfig& event_config)
    {
        if (!has_binding) {
            return;
        }
        fields_[binding.key] = binding;
        if (binding.has_getter) {
            getter_index_[binding.getter_method_id] = binding.key;
        }
        if (binding.has_setter) {
            setter_index_[binding.setter_method_id] = binding.key;
        }
        if (has_value) {
            field_values_[binding.key] = value;
        }
        if (has_event) {
            registered_events_[binding.notifier_event_id] = event_config;
        }
    }

    bool register_event_locked(const EventConfig& config)
    {
        if (!config.is_field) {
            if (id_in_use_locked(config.event_id, false, 0)) {
                return false;
            }
            if (registered_events_.size() >= registered_events_.max_size()) {
                return false;
            }
            registered_events_[config.event_id] = config;
            return true;
        }

        if (!field_has_accessor(config)) {
            return false;
        }

        const uint16_t new_key = field_storage_key(config);
        bool replacing = false;
        uint16_t old_key = 0;
        if (config.has_notifier) {
            const auto existing_event = registered_events_.find(config.event_id);
            if (existing_event != registered_events_.end() && !existing_event->second.is_field &&
                fields_.find(config.event_id) == fields_.end()) {
                return false;
            }
            if (fields_.find(config.event_id) != fields_.end()) {
                replacing = true;
                old_key = config.event_id;
            }
        }
        if (!replacing && fields_.find(new_key) != fields_.end()) {
            replacing = true;
            old_key = new_key;
        }
        if (!accessors_available_locked(config, replacing, old_key)) {
            return false;
        }

        if (!replacing) {
            if (fields_.size() >= fields_.max_size()) {
                return false;
            }
            if (config.has_notifier &&
                registered_events_.find(config.event_id) == registered_events_.end() &&
                registered_events_.size() >= registered_events_.max_size()) {
                return false;
            }
            if (!bind_field_locked(config)) {
                return false;
            }
            if (config.has_notifier) {
                registered_events_[config.event_id] = config;
            }
            return true;
        }

        FieldBinding saved_binding{};
        bool saved_binding_ok = false;
        const auto saved_it = fields_.find(old_key);
        if (saved_it != fields_.end()) {
            saved_binding = saved_it->second;
            saved_binding_ok = true;
        }
        platform::ByteBuffer saved_value;
        bool saved_value_ok = false;
        const auto value_it = field_values_.find(old_key);
        if (value_it != field_values_.end()) {
            saved_value = value_it->second;
            saved_value_ok = true;
        }
        bool saved_event_ok = false;
        EventConfig saved_event;
        if (saved_binding_ok && saved_binding.has_notifier) {
            const auto event_it = registered_events_.find(saved_binding.notifier_event_id);
            if (event_it != registered_events_.end()) {
                saved_event = event_it->second;
                saved_event_ok = true;
            }
        }

        if (saved_binding_ok) {
            unbind_field_locked(old_key);
        }
        if (old_key != new_key) {
            field_values_.erase(old_key);
            if (saved_binding_ok && saved_binding.has_notifier) {
                registered_events_.erase(saved_binding.notifier_event_id);
            }
        }

        const bool notifier_needs_slot =
            config.has_notifier &&
            registered_events_.find(config.event_id) == registered_events_.end();
        if (notifier_needs_slot && registered_events_.size() >= registered_events_.max_size()) {
            restore_field_locked(saved_binding_ok, saved_binding, saved_value_ok, saved_value,
                                 saved_event_ok, saved_event);
            return false;
        }
        if (!bind_field_locked(config)) {
            restore_field_locked(saved_binding_ok, saved_binding, saved_value_ok, saved_value,
                                 saved_event_ok, saved_event);
            return false;
        }
        if (saved_value_ok) {
            field_values_[new_key] = std::move(saved_value);
        }
        if (config.has_notifier) {
            registered_events_[config.event_id] = config;
        }
        return true;
    }

    void send_field_result(const MessagePtr& request, const transport::Endpoint& sender,
                           MessageType type, ReturnCode code, const platform::ByteBuffer& payload,
                           bool stamp_configured_version)
    {
        const MessageId response_id(request->get_service_id(), request->get_method_id());
        Message response(response_id, request->get_request_id(), type, code);
        if (stamp_configured_version) {
            response.set_interface_version(SOMEIP_INTERFACE_VERSION);
        }
        else {
            response.set_interface_version(request->get_interface_version());
        }
        response.set_payload(payload);
        (void)transport_.send_message(response, sender);
    }

    /** @implements REQ_MSG_123, REQ_MSG_123_E01, REQ_MSG_124, REQ_MSG_124_E01 */
    void handle_field_request(const MessagePtr& message, const transport::Endpoint& sender)
    {
        if (!message || message->get_service_id() != service_id_) {
            return;
        }
        if (message->get_message_type() != MessageType::REQUEST) {
            return;
        }

        const uint16_t method_id = message->get_method_id();
        FieldBinding binding;
        bool is_getter = false;
        bool is_setter = false;
        platform::ByteBuffer current;
        bool has_value = false;
        FieldSetter setter;
        {
            platform::ScopedLock const events_lock(events_mutex_);
            const auto getter_it = getter_index_.find(method_id);
            const auto setter_it = setter_index_.find(method_id);
            if (getter_it != getter_index_.end()) {
                const auto field_it = fields_.find(getter_it->second);
                if (field_it != fields_.end()) {
                    binding = field_it->second;
                    is_getter = true;
                }
            }
            else if (setter_it != setter_index_.end()) {
                const auto field_it = fields_.find(setter_it->second);
                if (field_it != fields_.end()) {
                    binding = field_it->second;
                    is_setter = true;
                }
            }
            if (is_getter || is_setter) {
                const auto value_it = field_values_.find(binding.key);
                if (value_it != field_values_.end()) {
                    current = value_it->second;
                    has_value = true;
                }
                setter = field_setter_;
            }
        }

        if (!is_getter && !is_setter) {
            send_field_result(message, sender, MessageType::ERROR, ReturnCode::E_UNKNOWN_METHOD, {},
                              false);
            return;
        }
        if (message->get_interface_version() != SOMEIP_INTERFACE_VERSION) {
            send_field_result(message, sender, MessageType::ERROR,
                              ReturnCode::E_WRONG_INTERFACE_VERSION, {}, false);
            return;
        }
        if (is_getter) {
            if (!message->get_payload().empty()) {
                send_field_result(message, sender, MessageType::ERROR,
                                  ReturnCode::E_MALFORMED_MESSAGE, {}, false);
                return;
            }
            const platform::ByteBuffer empty;
            send_field_result(message, sender, MessageType::RESPONSE, ReturnCode::E_OK,
                              has_value ? current : empty, true);
            return;
        }

        platform::ByteBuffer accepted = message->get_payload();
        FieldSetOutcome outcome = FieldSetOutcome::ACCEPTED;
        if (setter) {
            outcome = setter(method_id, message->get_payload(), accepted);
        }
        if (outcome == FieldSetOutcome::REJECTED) {
            platform::ByteBuffer latest;
            {
                platform::ScopedLock const events_lock(events_mutex_);
                const auto value_it = field_values_.find(binding.key);
                if (value_it != field_values_.end()) {
                    latest = value_it->second;
                }
            }
            send_field_result(message, sender, MessageType::ERROR, ReturnCode::E_NOT_OK, latest,
                              false);
            return;
        }

        bool still_present = false;
        bool notify = false;
        uint16_t notifier_id = 0;
        bool changed = true;
        {
            platform::ScopedLock const events_lock(events_mutex_);
            FieldBinding latest_binding;
            if (find_field_locked(method_id, latest_binding) && latest_binding.has_setter &&
                latest_binding.setter_method_id == method_id) {
                if (field_values_.size() >= field_values_.max_size() &&
                    field_values_.find(latest_binding.key) == field_values_.end()) {
                    still_present = false;
                }
                else {
                    const auto value_it = field_values_.find(latest_binding.key);
                    changed = value_it == field_values_.end() || value_it->second != accepted;
                    field_values_[latest_binding.key] = accepted;
                    still_present = true;
                    notify = latest_binding.has_notifier;
                    notifier_id = latest_binding.notifier_event_id;
                }
            }
        }
        if (!still_present) {
            send_field_result(message, sender, MessageType::ERROR, ReturnCode::E_UNKNOWN_METHOD, {},
                              false);
            return;
        }
        send_field_result(message, sender, MessageType::RESPONSE, ReturnCode::E_OK, accepted, true);
        if (notify && changed) {
            (void)publish_event(notifier_id, accepted);
        }
    }

    uint16_t service_id_;
    uint16_t instance_id_;
    platform::String<> default_client_address_{"0.0.0.0"};
    uint16_t default_client_port_{0};
    transport::detail::TransportSession transport_session_;
    transport::ITransport& transport_;

    platform::UnorderedMap<uint16_t, EventConfig, 16> registered_events_;
    platform::UnorderedMap<uint16_t, platform::ByteBuffer, 16> field_values_;
    platform::UnorderedMap<uint16_t, FieldBinding, 16> fields_;
    platform::UnorderedMap<uint16_t, uint16_t, 16> getter_index_;
    platform::UnorderedMap<uint16_t, uint16_t, 16> setter_index_;
    FieldSetter field_setter_;
    mutable platform::Mutex events_mutex_;

    platform::UnorderedMap<uint16_t, platform::Vector<ClientInfo, 8>, 16> subscriptions_;
    mutable platform::Mutex subscriptions_mutex_;

    platform::UnorderedMap<uint16_t, std::chrono::steady_clock::time_point, 16> last_publish_times_;
    std::optional<platform::Thread> publish_timer_thread_;
    std::atomic<uint16_t> next_session_id_;
    std::atomic<bool> running_;

    platform::Function<void(const transport::MessageRejectionInfo&)> rejection_handler_;
    mutable platform::Mutex rejection_mutex_;
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(sizeof(EventPublisherImpl) <= SOMEIP_PIMPL_EVENTPUB_SIZE,
              "EventPublisherImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_EVENTPUB_SIZE");
#endif

// EventPublisher implementation
EventPublisher::EventPublisher(uint16_t service_id, uint16_t instance_id)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_)
        EventPublisherImpl(service_id, instance_id, transport::Endpoint("0.0.0.0", 0));
}
#else
    : impl_(std::make_unique<EventPublisherImpl>(service_id, instance_id,
                                                 transport::Endpoint("0.0.0.0", 0)))
{
}
#endif

EventPublisher::EventPublisher(uint16_t service_id, uint16_t instance_id,
                               transport::ITransport& transport)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) EventPublisherImpl(service_id, instance_id, transport);
}
#else
    : impl_(std::make_unique<EventPublisherImpl>(service_id, instance_id, transport))
{
}
#endif

EventPublisher::~EventPublisher()
{
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~EventPublisherImpl();
#endif
}

bool EventPublisher::initialize()
{
    return impl()->initialize();
}

void EventPublisher::shutdown()
{
    impl()->shutdown();
}

Result EventPublisher::get_transport_result() const
{
    return impl()->get_transport_result();
}

bool EventPublisher::register_event(const EventConfig& config)
{
    return impl()->register_event(config);
}

bool EventPublisher::unregister_event(uint16_t event_id)
{
    return impl()->unregister_event(event_id);
}

bool EventPublisher::update_event_config(uint16_t event_id, const EventConfig& config)
{
    return impl()->update_event_config(event_id, config);
}

bool EventPublisher::publish_event(uint16_t event_id, const platform::ByteBuffer& data)
{
    return impl()->publish_event(event_id, data);
}

bool EventPublisher::publish_field(uint16_t event_id, const platform::ByteBuffer& data)
{
    return impl()->publish_field(event_id, data);
}

transport::Endpoint EventPublisher::get_local_endpoint() const
{
    return impl()->get_local_endpoint();
}

void EventPublisher::set_field_setter(FieldSetter setter)
{
    impl()->set_field_setter(std::move(setter));
}

void EventPublisher::set_default_client_endpoint(const platform::String<>& address, uint16_t port)
{
    impl()->set_default_client_endpoint(address, port);
}

bool EventPublisher::handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                                         const platform::Vector<EventFilter>& filters)
{
    return impl()->handle_subscription(eventgroup_id, client_id, filters);
}

bool EventPublisher::handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                                         uint32_t ttl_seconds,
                                         const platform::Vector<EventFilter>& filters)
{
    return impl()->handle_subscription(eventgroup_id, client_id, ttl_seconds, filters);
}

bool EventPublisher::handle_subscription(uint16_t eventgroup_id, uint16_t client_id,
                                         uint32_t ttl_seconds,
                                         const platform::Vector<EventFilter>& filters,
                                         bool send_initial_events)
{
    return impl()->handle_subscription(eventgroup_id, client_id, ttl_seconds, filters,
                                       send_initial_events);
}

bool EventPublisher::handle_unsubscription(uint16_t eventgroup_id, uint16_t client_id)
{
    return impl()->handle_unsubscription(eventgroup_id, client_id);
}

size_t EventPublisher::cleanup_expired_subscriptions()
{
    return impl()->cleanup_expired_subscriptions();
}

platform::Vector<uint16_t> EventPublisher::get_registered_events() const
{
    return impl()->get_registered_events();
}

platform::Vector<uint16_t> EventPublisher::get_subscriptions(uint16_t eventgroup_id) const
{
    return impl()->get_subscriptions(eventgroup_id);
}

bool EventPublisher::is_ready() const
{
    return impl()->is_ready();
}

void EventPublisher::set_message_rejection_handler(
    platform::Function<void(const transport::MessageRejectionInfo&)> handler)
{
    impl()->set_message_rejection_handler(std::move(handler));
}

EventPublisher::Statistics EventPublisher::get_statistics() const
{
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::events
