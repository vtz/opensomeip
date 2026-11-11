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

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>  // NOLINT(misc-include-cleaner) - static allocation placement new
#include <optional>
#include <unordered_map>
#include <utility>

#include "../common/callback_storage.h"
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

namespace {
/**
 * @brief A remote application destination has a valid address and a non-zero port.
 *
 * A default-constructed endpoint is unresolved (invalid) and must not be sent.
 * Port 0 is an ephemeral local bind, not a remote application port.
 *
 * @implements REQ_TRANSPORT_014
 */
bool is_resolved_remote_destination(const transport::Endpoint& endpoint)
{
    return endpoint.is_valid() && endpoint.get_port() != 0;
}

void uint16_to_str(uint16_t val, platform::String<>& out)
{
    if (val == 0) {
        out.append("0");
        return;
    }
    std::array<char, 6> digits{};
    int pos = 5;
    while (val > 0) {
        --pos;
        digits.at(static_cast<size_t>(pos)) = static_cast<char>('0' + (val % 10));
        val /= 10;
    }
    out.append(digits.data() + pos, digits.data() + 5);
}

}  // namespace

// NOLINTBEGIN(misc-include-cleaner) - platform::Mutex from platform/thread.h (IWYU false positives
// in impl).

/**
 * @brief Event Subscriber implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @satisfies feat_req_someip_730
 * @satisfies feat_req_someip_731
 */
class EventSubscriberImpl : public transport::ITransportListener {
    class DispatchFrame {
       public:
        explicit DispatchFrame(EventSubscriberImpl& owner)
            : owner_(owner), owner_thread_(platform::this_thread::get_id())
        {
            platform::ScopedLock const lock(owner_.dispatch_mutex_);
            // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer) - requires the lock
            ticket_ = ++owner_.next_dispatch_ticket_;
            // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer) - requires the lock
            next_ = owner_.active_dispatches_;
            owner_.active_dispatches_ = this;
        }
        ~DispatchFrame()
        {
            platform::ScopedLock const lock(owner_.dispatch_mutex_);
            auto** entry = &owner_.active_dispatches_;
            while (*entry != this) {
                entry = &(*entry)->next_;
            }
            *entry = next_;
            // notify_all(): shutdown() and an external unsubscribe() can both be
            // parked on dispatch_drained_ at the same time; notify_one() could
            // wake the wrong one and leave the other stuck.
            owner_.dispatch_drained_.notify_all();
        }
        DispatchFrame(const DispatchFrame&) = delete;
        DispatchFrame& operator=(const DispatchFrame&) = delete;
        DispatchFrame(DispatchFrame&&) = delete;
        DispatchFrame& operator=(DispatchFrame&&) = delete;

        /**
         * @brief Record the subscription key this notification matched, if any.
         *
         * Called at most once, after the subscriptions_mutex_ lookup in
         * on_message_received. Left empty when no subscription matched.
         * Guarded by owner_.dispatch_mutex_ so unsubscribe_eventgroup() can
         * safely read it from a different thread.
         */
        void set_matched_key(platform::String<> key)
        {
            platform::ScopedLock const lock(owner_.dispatch_mutex_);
            matched_key_ = std::move(key);
        }

        /**
         * @brief True if the calling thread is already inside a dispatch for
         *        this owner (i.e. this call is reentrant from a notification
         *        callback belonging to the same EventSubscriberImpl).
         *
         * Reentrancy is detected per-thread: a DIFFERENT thread synchronously
         * waiting on a notification callback (e.g. joining a worker it
         * offloaded the callback to) is NOT recognized as reentrant and will
         * take the external, blocking path below -- see the @note on
         * EventSubscriber::unsubscribe_eventgroup for the resulting deadlock
         * hazard.
         */
        static bool reentrant(const EventSubscriberImpl& owner)
        {
            platform::ScopedLock const lock(owner.dispatch_mutex_);
            const platform::ThreadId self = platform::this_thread::get_id();
            for (auto* frame = owner.active_dispatches_; frame != nullptr; frame = frame->next_) {
                if (&frame->owner_ == &owner && frame->owner_thread_ == self) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief True if a still-active frame was admitted at or before
         *        @p cutoff AND matched @p key, i.e. an unsubscribe() for that
         *        exact subscription must keep waiting.
         *
         * Callers must already hold owner_.dispatch_mutex_ (this is invoked
         * from inside the dispatch_drained_ wait predicate).
         */
        static bool pending(const DispatchFrame* frame, uint64_t cutoff,
                            const platform::String<>& key)
        {
            for (; frame != nullptr; frame = frame->next_) {
                if (frame->ticket_ <= cutoff && frame->matched_key_ == key) {
                    return true;
                }
            }
            return false;
        }

       private:
        EventSubscriberImpl& owner_;
        uint64_t ticket_{0};
        DispatchFrame* next_{nullptr};
        platform::ThreadId owner_thread_;
        // Guarded by owner_.dispatch_mutex_.
        platform::String<> matched_key_;
    };

   public:
    template <typename Transport>
    EventSubscriberImpl(uint16_t client_id, Transport&& transport)
        : client_id_(client_id),
          transport_session_(std::forward<Transport>(transport)),
          transport_(transport_session_.get()),
          running_(false)
    {
    }

    ~EventSubscriberImpl() noexcept override
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

    EventSubscriberImpl(const EventSubscriberImpl&) = delete;
    EventSubscriberImpl& operator=(const EventSubscriberImpl&) = delete;
    EventSubscriberImpl(EventSubscriberImpl&&) = delete;
    EventSubscriberImpl& operator=(EventSubscriberImpl&&) = delete;

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
        return true;
    }

    void shutdown()
    {
        if (!running_) {
            transport_session_.stop();
            return;
        }

        running_ = false;
        transport_session_
            .stop();  // Guarantees no NEW DispatchFrame is admitted after this returns.

        // Drain any DispatchFrame still running a notification callback before
        // tearing down dispatch_mutex_/dispatch_drained_ (used by ~DispatchFrame).
        {
            platform::ScopedLock const dispatch_lock(dispatch_mutex_);
            dispatch_drained_.wait(dispatch_mutex_, [&] { return active_dispatches_ == nullptr; });
        }

        // Hand off with any external unsubscribe_eventgroup() call: it holds
        // unsubscribe_mutex_ for the entirety of its own dispatch_drained_ wait,
        // so acquiring (and immediately releasing) it here proves none is still
        // in flight before subscriptions_/field_requests_ are torn down.
        {
            platform::ScopedLock const unsubscribe_lock(unsubscribe_mutex_);
        }

        someip::detail::release_entries(subscriptions_, subscriptions_mutex_);
        someip::detail::release_entries(field_requests_, field_requests_mutex_);
    }

    /** @implements REQ_MSG_122, REQ_TRANSPORT_014, REQ_TRANSPORT_030 */
    Result subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                EventNotificationCallback notification_callback,
                                SubscriptionStatusCallback status_callback,
                                const platform::Vector<EventFilter>& filters)
    {
        if (!running_) {
            return Result::INVALID_STATE;
        }

        EventSubscription subscription(service_id, instance_id, 0, eventgroup_id);
        subscription.state = SubscriptionState::REQUESTED;

        SubscriptionInfo sub_info;
        sub_info.subscription = subscription;
        sub_info.notification_callback = std::move(notification_callback);
        sub_info.status_callback = std::move(status_callback);
        sub_info.filters = filters;

        transport::Endpoint service_endpoint;
        bool renewing = false;
        {
            // Admission only. The new entry is published after resolve and send
            // both succeed, and this mutex is not held across send_message.
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            if (!running_) {
                return Result::INVALID_STATE;
            }
            const platform::String<> key =
                make_subscription_key(service_id, instance_id, eventgroup_id);
            for (const auto& entry : subscriptions_) {
                if (entry.second.subscription.service_id == service_id && entry.first != key) {
                    return Result::SUBSCRIPTION_FAILED;
                }
            }
            renewing = subscriptions_.find(key) != subscriptions_.end();
            if (!renewing && subscriptions_.size() >= subscriptions_.max_size()) {
                return Result::SUBSCRIPTION_FAILED;
            }
            service_endpoint = resolve_service_endpoint(service_id, instance_id);
            // Unresolved destinations fail before send. Do not target the SD port
            // and do not erase a live exact-key subscription.
            if (!is_resolved_remote_destination(service_endpoint)) {
                return Result::INVALID_ENDPOINT;
            }
        }

        MessageId const msg_id(service_id, 0x0001);  // Method ID for subscription
        Message subscription_msg(msg_id, RequestId(client_id_, 0x0001), MessageType::REQUEST,
                                 ReturnCode::E_OK);

        platform::ByteBuffer payload;
        payload.push_back(
            static_cast<uint8_t>((static_cast<uint32_t>(eventgroup_id) >> 8U) & 0xFFU));
        payload.push_back(static_cast<uint8_t>(static_cast<uint32_t>(eventgroup_id) & 0xFFU));
        subscription_msg.set_payload(payload);

        const Result send_result = transport_.send_message(subscription_msg, service_endpoint);
        if (send_result != Result::SUCCESS) {
            return send_result;
        }

        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            if (!running_) {
                return Result::INVALID_STATE;
            }
            const platform::String<> key =
                make_subscription_key(service_id, instance_id, eventgroup_id);
            for (const auto& entry : subscriptions_) {
                if (entry.second.subscription.service_id == service_id && entry.first != key) {
                    return Result::SUBSCRIPTION_FAILED;
                }
            }
            const auto existing = subscriptions_.find(key);
            if (renewing && existing == subscriptions_.end()) {
                // Removed while the renewal was in flight. Do not resurrect it.
                return Result::INVALID_STATE;
            }
            if (!renewing && existing == subscriptions_.end() &&
                subscriptions_.size() >= subscriptions_.max_size()) {
                return Result::SUBSCRIPTION_FAILED;
            }
            subscriptions_[key] = std::move(sub_info);
        }
        return Result::SUCCESS;
    }

    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id)
    {
        if (DispatchFrame::reentrant(*this)) {
            return remove_subscription(service_id, instance_id, eventgroup_id);
        }
        // One external waiter avoids relying on a PAL-wide broadcast operation.
        platform::ScopedLock const unsubscribe_lock(unsubscribe_mutex_);
        const platform::String<> key =
            make_subscription_key(service_id, instance_id, eventgroup_id);
        uint64_t cutoff = 0;
        const bool removed = remove_subscription(service_id, instance_id, eventgroup_id, &cutoff);
        // Always wait, whether or not this call actually erased the entry: a
        // reentrant call from this subscription's own callback may have erased
        // it moments ago while its DispatchFrame is still on the stack, and
        // that callback's captured state must not be freed out from under it.
        platform::ScopedLock const dispatch_lock(dispatch_mutex_);
        dispatch_drained_.wait(dispatch_mutex_, [&] {
            return !DispatchFrame::pending(active_dispatches_, cutoff, key);
        });
        return removed;
    }

   private:
    bool remove_subscription(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                             uint64_t* cutoff = nullptr)
    {
        bool found = false;
        bool should_send = false;
        transport::Endpoint service_endpoint;
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            if (running_) {
                const platform::String<> key =
                    make_subscription_key(service_id, instance_id, eventgroup_id);

                auto it = subscriptions_.find(key);
                if (it != subscriptions_.end()) {
                    found = true;

                    // Best-effort: the unsubscribe message is sent when the endpoint
                    // resolves, but the local subscription is erased either way so a
                    // subscription whose service never resolves a port isn't stuck
                    // forever (it was never reachable to notify in the first place).
                    service_endpoint = resolve_service_endpoint(service_id, instance_id);
                    should_send = is_resolved_remote_destination(service_endpoint);
                    subscriptions_.erase(it);
                }
            }
            if (cutoff != nullptr) {
                // Keep removal and cutoff sampling atomic with respect to resubscription.
                // Dispatch never holds dispatch_mutex_ while acquiring subscriptions_mutex_.
                platform::ScopedLock const dispatch_lock(dispatch_mutex_);
                *cutoff = next_dispatch_ticket_;
            }
        }
        // Send after releasing subscriptions_mutex_. An injected transport may
        // deliver on this thread, and the listener takes that mutex.
        if (should_send) {
            MessageId const msg_id(service_id, 0x0002);
            Message unsubscription_msg(msg_id, RequestId(client_id_, 0x0002), MessageType::REQUEST,
                                       ReturnCode::E_OK);

            platform::ByteBuffer payload;
            payload.push_back(
                static_cast<uint8_t>((static_cast<uint32_t>(eventgroup_id) >> 8U) & 0xFFU));
            payload.push_back(static_cast<uint8_t>(static_cast<uint32_t>(eventgroup_id) & 0xFFU));
            unsubscription_msg.set_payload(payload);

            (void)transport_.send_message(unsubscription_msg, service_endpoint);
        }
        return found;
    }

   public:
    /** @implements REQ_MSG_123, REQ_TRANSPORT_014 */
    bool request_field(uint16_t service_id, uint16_t instance_id, uint16_t getter_method_id,
                       EventNotificationCallback callback, uint8_t interface_version)
    {
        const platform::ByteBuffer empty;
        return send_field_request(service_id, instance_id, getter_method_id, empty,
                                  std::move(callback), interface_version);
    }

    /** @implements REQ_MSG_124, REQ_TRANSPORT_014 */
    bool set_field(uint16_t service_id, uint16_t instance_id, uint16_t setter_method_id,
                   const platform::ByteBuffer& value, EventNotificationCallback callback,
                   uint8_t interface_version)
    {
        return send_field_request(service_id, instance_id, setter_method_id, value,
                                  std::move(callback), interface_version);
    }

    bool cancel_field(uint16_t service_id, uint16_t method_id)
    {
        const platform::String<> key = make_field_key(service_id, method_id);
        EventNotificationCallback callback;
        EventNotification notification;
        {
            platform::ScopedLock const field_lock(field_requests_mutex_);
            auto it = field_requests_.find(key);
            if (it == field_requests_.end()) {
                return false;
            }
            notification = EventNotification(service_id, it->second.instance_id, method_id);
            notification.client_id = client_id_;
            notification.session_id = it->second.session_id;
            notification.message_type = MessageType::ERROR;
            notification.return_code = ReturnCode::E_TIMEOUT;
            callback = std::move(it->second.callback);
            field_requests_.erase(it);
        }
        if (callback) {
            const DispatchFrame dispatch(*this);
            callback(notification);
        }
        return true;
    }

    bool send_field_request(uint16_t service_id, uint16_t instance_id, uint16_t method_id,
                            const platform::ByteBuffer& payload, EventNotificationCallback callback,
                            uint8_t interface_version)
    {
        if (!running_) {
            return false;
        }

        transport::Endpoint service_endpoint;
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            service_endpoint = resolve_service_endpoint(service_id, instance_id);
        }
        if (!is_resolved_remote_destination(service_endpoint)) {
            return false;
        }

        const platform::String<> key = make_field_key(service_id, method_id);
        uint64_t ticket = 0;
        uint16_t session_id = 0;
        EventNotificationCallback expired_callback;
        EventNotification expired_notification;
        {
            platform::ScopedLock const field_lock(field_requests_mutex_);
            if (!running_) {
                return false;
            }
            auto existing = field_requests_.find(key);
            if (existing != field_requests_.end()) {
                if (std::chrono::steady_clock::now() < existing->second.deadline) {
                    return false;
                }
                expired_notification =
                    EventNotification(existing->second.service_id, existing->second.instance_id,
                                      existing->second.method_id);
                expired_notification.client_id = client_id_;
                expired_notification.session_id = existing->second.session_id;
                expired_notification.message_type = MessageType::ERROR;
                expired_notification.return_code = ReturnCode::E_TIMEOUT;
                expired_callback = std::move(existing->second.callback);
                field_requests_.erase(existing);
            }
            if (field_requests_.size() >= field_requests_.max_size()) {
                return false;
            }
            ticket = next_field_ticket_++;
            session_id = next_field_session_id_;
            next_field_session_id_ = (session_id == 0xFFFFU)
                                         ? static_cast<uint16_t>(1U)
                                         : static_cast<uint16_t>(session_id + 1U);
            FieldRequest request;
            request.ticket = ticket;
            request.session_id = session_id;
            request.instance_id = instance_id;
            request.service_id = service_id;
            request.method_id = method_id;
            request.callback = std::move(callback);
            field_requests_[key] = std::move(request);
        }

        if (expired_callback) {
            const DispatchFrame dispatch(*this);
            expired_callback(expired_notification);
        }

        MessageId const msg_id(service_id, method_id);
        Message field_msg(msg_id, RequestId(client_id_, session_id), MessageType::REQUEST,
                          ReturnCode::E_OK);
        field_msg.set_interface_version(interface_version);
        field_msg.set_payload(payload);

        if (transport_.send_message(field_msg, service_endpoint) != Result::SUCCESS) {
            platform::ScopedLock const field_lock(field_requests_mutex_);
            const auto it = field_requests_.find(key);
            if (it != field_requests_.end() && it->second.ticket == ticket) {
                field_requests_.erase(it);
            }
            return false;
        }
        return true;
    }

    bool set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                           const platform::Vector<EventFilter>& filters)
    {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        platform::String<> const key =
            make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return false;
        }

        it->second.filters = filters;
        return true;
    }

    platform::Vector<EventSubscription> get_active_subscriptions() const
    {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        platform::Vector<EventSubscription> result;
        result.reserve(subscriptions_.size());

        for (const auto& pair : subscriptions_) {
            result.push_back(pair.second.subscription);
        }

        return result;
    }

    SubscriptionState get_subscription_status(uint16_t service_id, uint16_t instance_id,
                                              uint16_t eventgroup_id) const
    {
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        platform::String<> const key =
            make_subscription_key(service_id, instance_id, eventgroup_id);

        auto it = subscriptions_.find(key);
        if (it == subscriptions_.end()) {
            return SubscriptionState::REQUESTED;
        }

        return it->second.subscription.state;
    }

    bool is_ready() const
    {
        return running_ && transport_.is_connected();
    }

    EventSubscriber::Statistics get_statistics() const
    {
        // TODO: Implement statistics tracking
        return EventSubscriber::Statistics{};
    }

    using EndpointResolver = platform::Function<transport::Endpoint(uint16_t, uint16_t)>;

    void set_endpoint_resolver(EndpointResolver resolver)
    {
        platform::ScopedLock const lock(subscriptions_mutex_);
        endpoint_resolver_ = std::move(resolver);
    }

    void set_default_endpoint(const platform::String<>& address, uint16_t port)
    {
        platform::ScopedLock const lock(subscriptions_mutex_);
        default_service_address_ = address;
        default_service_port_ = port;
    }

    transport::Endpoint get_local_endpoint() const
    {
        return transport_.get_local_endpoint();
    }

    void set_message_rejection_handler(
        platform::Function<void(const transport::MessageRejectionInfo&)> handler)
    {
        platform::ScopedLock const lock(rejection_mutex_);
        rejection_handler_ = std::move(handler);
    }

   private:
    struct SubscriptionInfo {
        EventSubscription subscription;
        EventNotificationCallback notification_callback;
        SubscriptionStatusCallback status_callback;
        platform::Vector<EventFilter> filters;
    };

    transport::Endpoint resolve_service_endpoint(uint16_t service_id, uint16_t instance_id) const
    {
        if (endpoint_resolver_) {
            return endpoint_resolver_(service_id, instance_id);
        }
        // No resolver and no set_default_endpoint(): unresolved. Do not substitute
        // a fixed application port or the SD port.
        if (default_service_address_ == "0.0.0.0" && default_service_port_ == 0) {
            return transport::Endpoint();
        }
        return transport::Endpoint(default_service_address_, default_service_port_);
    }

    platform::String<> make_subscription_key(uint16_t service_id, uint16_t instance_id,
                                             uint16_t eventgroup_id) const
    {
        platform::String<> key;
        uint16_to_str(service_id, key);
        key.append(":");
        uint16_to_str(instance_id, key);
        key.append(":");
        uint16_to_str(eventgroup_id, key);
        return key;
    }

    // Keyed by service and getter/setter method id. Instance id is not in the
    // SOME/IP header, so the response is matched on service, method, and session.
    platform::String<> make_field_key(uint16_t service_id, uint16_t method_id)
    {
        platform::String<> key;
        uint16_to_str(service_id, key);
        key.append(":");
        uint16_to_str(method_id, key);
        return key;
    }

    struct ExpiredField {
        EventNotificationCallback callback;
        EventNotification notification;
    };

    void sweep_expired_fields_locked(platform::Vector<ExpiredField, 8>& expired)
    {
        const auto now = std::chrono::steady_clock::now();
        platform::Vector<platform::String<>, 8> keys_to_erase;
        for (auto& entry : field_requests_) {
            if (now >= entry.second.deadline) {
                ExpiredField item;
                item.notification = EventNotification(entry.second.service_id,
                                                      entry.second.instance_id,
                                                      entry.second.method_id);
                item.notification.client_id = client_id_;
                item.notification.session_id = entry.second.session_id;
                item.notification.message_type = MessageType::ERROR;
                item.notification.return_code = ReturnCode::E_TIMEOUT;
                item.callback = std::move(entry.second.callback);
                expired.push_back(std::move(item));
                keys_to_erase.push_back(entry.first);
            }
        }
        for (const auto& k : keys_to_erase) {
            field_requests_.erase(k);
        }
    }

    void fire_expired_fields(platform::Vector<ExpiredField, 8>& expired)
    {
        for (auto& item : expired) {
            if (item.callback) {
                const DispatchFrame dispatch(*this);
                item.callback(item.notification);
            }
        }
    }

    void handle_field_response(const MessagePtr& message)
    {
        if (!message || message->get_client_id() != client_id_) {
            return;
        }
        const platform::String<> key =
            make_field_key(message->get_service_id(), message->get_method_id());
        EventNotificationCallback callback;
        EventNotification notification;
        platform::Vector<ExpiredField, 8> expired;
        {
            platform::ScopedLock const field_lock(field_requests_mutex_);
            const auto it = field_requests_.find(key);
            if (it == field_requests_.end() || it->second.session_id != message->get_session_id()) {
                sweep_expired_fields_locked(expired);
                // Release lock, then fire expired callbacks below.
            }
            else {
                notification = EventNotification(message->get_service_id(), it->second.instance_id,
                                                 message->get_method_id());
                notification.client_id = message->get_client_id();
                notification.session_id = message->get_session_id();
                notification.event_data = message->get_payload();
                notification.message_type = message->get_message_type();
                notification.return_code = message->get_return_code();
                notification.interface_version = message->get_interface_version();
                callback = std::move(it->second.callback);
                field_requests_.erase(it);
                sweep_expired_fields_locked(expired);
            }
        }
        fire_expired_fields(expired);
        if (callback) {
            const DispatchFrame dispatch(*this);
            callback(notification);
        }
    }

    void on_message_received(MessagePtr message, const transport::Endpoint& /*sender*/) override
    {
        if (!message) {
            return;
        }
        const MessageType type = message->get_message_type();
        if (type == MessageType::RESPONSE || type == MessageType::ERROR) {
            handle_field_response(message);
            return;
        }
        if (type != MessageType::NOTIFICATION) {
            return;
        }
        // Registered before snapshots; destroyed after local callable copies, including unwind.
        // Non-const: the matched subscription key is recorded onto it below so
        // unsubscribe_eventgroup() can scope its barrier wait to this subscription.
        DispatchFrame dispatch(*this);

        uint16_t const service_id = message->get_service_id();
        uint16_t const event_id =
            message->get_method_id();  // Event ID is in method ID field for notifications

        EventNotificationCallback notification_callback;
        std::optional<EventNotification> notification;
        uint16_t notified_instance_id = 0;
        platform::String<> matched_key;
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            for (auto& sub_pair : subscriptions_) {
                auto& sub_info = sub_pair.second;
                if (sub_info.subscription.service_id == service_id) {
                    notified_instance_id = sub_info.subscription.instance_id;
                    notification.emplace(service_id, notified_instance_id, event_id);
                    notification->client_id = message->get_client_id();
                    notification->session_id = message->get_session_id();
                    notification_callback = sub_info.notification_callback;
                    sub_info.subscription.state = SubscriptionState::SUBSCRIBED;
                    sub_info.subscription.last_notification = std::chrono::steady_clock::now();
                    matched_key = sub_pair.first;
                    break;
                }
            }
            // Publish the key while subscriptions_mutex_ is STILL held. This is
            // load-bearing, not tidiness: remove_subscription() holds this same
            // mutex across both the erase and the cutoff read, so publishing here
            // makes "this frame snapshotted the callback" and "unsubscribe chose a
            // cutoff" strictly ordered. Publishing after the unlock leaves a window
            // where a frame that already copied notification_callback still has an
            // empty key, so pending() reports nothing pending, unsubscribe returns,
            // the caller frees the captured state, and the callback then runs
            // against it. Left empty when no subscription matched, so a real
            // unsubscribe key never accidentally matches this frame.
            // Lock order subscriptions_mutex_ -> dispatch_mutex_ matches
            // remove_subscription(); no other path takes them the other way round.
            dispatch.set_matched_key(std::move(matched_key));
        }

        if (notification && notification_callback) {
            notification->event_data = message->get_payload();
            notification_callback(*notification);
        }
    }

    void on_connection_lost(const transport::Endpoint& /*endpoint*/) override
    {
        {
            platform::ScopedLock const subs_lock(subscriptions_mutex_);
            for (auto& sub_pair : subscriptions_) {
                auto& sub_info = sub_pair.second;
                if (sub_info.subscription.state == SubscriptionState::SUBSCRIBED) {
                    sub_info.subscription.state = SubscriptionState::PENDING;
                }
            }
        }

        platform::Vector<ExpiredField, 8> pending;
        {
            platform::ScopedLock const field_lock(field_requests_mutex_);
            for (auto& entry : field_requests_) {
                ExpiredField item;
                item.notification = EventNotification(entry.second.service_id,
                                                      entry.second.instance_id,
                                                      entry.second.method_id);
                item.notification.client_id = client_id_;
                item.notification.session_id = entry.second.session_id;
                item.notification.message_type = MessageType::ERROR;
                item.notification.return_code = ReturnCode::E_TIMEOUT;
                item.callback = std::move(entry.second.callback);
                pending.push_back(std::move(item));
            }
            field_requests_.clear();
        }
        fire_expired_fields(pending);
    }

    void on_connection_established(const transport::Endpoint& /*endpoint*/) override
    {
        // Handle service reconnection
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

    uint16_t client_id_;
    platform::String<> default_service_address_{"0.0.0.0"};
    uint16_t default_service_port_{0};
    EndpointResolver endpoint_resolver_;
    transport::detail::TransportSession transport_session_;
    transport::ITransport& transport_;

    platform::UnorderedMap<platform::String<>, SubscriptionInfo> subscriptions_;
    mutable platform::Mutex subscriptions_mutex_;

    static constexpr auto FIELD_REQUEST_TIMEOUT = std::chrono::seconds(5);

    struct FieldRequest {
        uint64_t ticket{0};
        uint16_t session_id{0};
        uint16_t instance_id{0};
        uint16_t service_id{0};
        uint16_t method_id{0};
        EventNotificationCallback callback;
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() +
                                                       FIELD_REQUEST_TIMEOUT};
    };

    platform::UnorderedMap<platform::String<>, FieldRequest> field_requests_;
    mutable platform::Mutex field_requests_mutex_;
    uint64_t next_field_ticket_{1};
    uint16_t next_field_session_id_{1};
    platform::Mutex unsubscribe_mutex_;
    mutable platform::Mutex dispatch_mutex_;
    platform::ConditionVariable dispatch_drained_;
    DispatchFrame* active_dispatches_{nullptr};
    uint64_t next_dispatch_ticket_{0};

    std::atomic<bool> running_;

    platform::Function<void(const transport::MessageRejectionInfo&)> rejection_handler_;
    mutable platform::Mutex rejection_mutex_;
};

#ifdef SOMEIP_STATIC_ALLOC
static_assert(
    sizeof(EventSubscriberImpl) <= SOMEIP_PIMPL_EVENTSUB_SIZE,
    "EventSubscriberImpl exceeds pimpl storage size; increase SOMEIP_PIMPL_EVENTSUB_SIZE");
#endif

// EventSubscriber implementation
EventSubscriber::EventSubscriber(uint16_t client_id)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) EventSubscriberImpl(client_id, transport::Endpoint("0.0.0.0", 0));
}
#else
    : impl_(std::make_unique<EventSubscriberImpl>(client_id, transport::Endpoint("0.0.0.0", 0)))
{
}
#endif

EventSubscriber::EventSubscriber(uint16_t client_id, transport::ITransport& transport)
#ifdef SOMEIP_STATIC_ALLOC
{
    new (impl_storage_) EventSubscriberImpl(client_id, transport);
}
#else
    : impl_(std::make_unique<EventSubscriberImpl>(client_id, transport))
{
}
#endif

EventSubscriber::~EventSubscriber()
{
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~EventSubscriberImpl();
#endif
}

void EventSubscriber::set_default_endpoint(const platform::String<>& address, uint16_t port)
{
    impl()->set_default_endpoint(address, port);
}

transport::Endpoint EventSubscriber::get_local_endpoint() const
{
    return impl()->get_local_endpoint();
}

void EventSubscriber::set_endpoint_resolver(EndpointResolver resolver)
{
    impl()->set_endpoint_resolver(std::move(resolver));
}

bool EventSubscriber::initialize()
{
    return impl()->initialize();
}

void EventSubscriber::shutdown()
{
    impl()->shutdown();
}

Result EventSubscriber::get_transport_result() const
{
    return impl()->get_transport_result();
}

Result EventSubscriber::subscribe_eventgroup(uint16_t service_id, uint16_t instance_id,
                                             uint16_t eventgroup_id,
                                             EventNotificationCallback notification_callback,
                                             SubscriptionStatusCallback status_callback,
                                             const platform::Vector<EventFilter>& filters)
{
    return impl()->subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                                        std::move(notification_callback),
                                        std::move(status_callback), filters);
}

bool EventSubscriber::unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id,
                                             uint16_t eventgroup_id)
{
    return impl()->unsubscribe_eventgroup(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::request_field(uint16_t service_id, uint16_t instance_id,
                                    uint16_t getter_method_id, EventNotificationCallback callback,
                                    uint8_t interface_version)
{
    return impl()->request_field(service_id, instance_id, getter_method_id, std::move(callback),
                                 interface_version);
}

bool EventSubscriber::set_field(uint16_t service_id, uint16_t instance_id,
                                uint16_t setter_method_id, const platform::ByteBuffer& value,
                                EventNotificationCallback callback, uint8_t interface_version)
{
    return impl()->set_field(service_id, instance_id, setter_method_id, value, std::move(callback),
                             interface_version);
}

bool EventSubscriber::cancel_field(uint16_t service_id, uint16_t method_id)
{
    return impl()->cancel_field(service_id, method_id);
}

bool EventSubscriber::set_event_filters(uint16_t service_id, uint16_t instance_id,
                                        uint16_t eventgroup_id,
                                        const platform::Vector<EventFilter>& filters)
{
    return impl()->set_event_filters(service_id, instance_id, eventgroup_id, filters);
}

platform::Vector<EventSubscription> EventSubscriber::get_active_subscriptions() const
{
    return impl()->get_active_subscriptions();
}

SubscriptionState EventSubscriber::get_subscription_status(uint16_t service_id,
                                                           uint16_t instance_id,
                                                           uint16_t eventgroup_id) const
{
    return impl()->get_subscription_status(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::is_ready() const
{
    return impl()->is_ready();
}

void EventSubscriber::set_message_rejection_handler(
    platform::Function<void(const transport::MessageRejectionInfo&)> handler)
{
    impl()->set_message_rejection_handler(std::move(handler));
}

EventSubscriber::Statistics EventSubscriber::get_statistics() const
{
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::events
