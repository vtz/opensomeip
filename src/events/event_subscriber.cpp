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
void uint16_to_str(uint16_t val, platform::String<>& out) {
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
    out.append(digits.data() + pos,
               digits.data() + 5);
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
        static bool pending(const DispatchFrame* frame, uint64_t cutoff, const platform::String<>& key)
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
        } catch (...) {}  // NOLINT(bugprone-empty-catch) destructor must not throw
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

    bool initialize() {
        if (running_) {
            return true;
        }

        if (transport_session_.start(*this) != Result::SUCCESS) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
        if (!running_) {
            transport_session_.stop();
            return;
        }

        running_ = false;
        transport_session_.stop();  // Guarantees no NEW DispatchFrame is admitted after this returns.

        // Drain any DispatchFrame still running a notification callback before
        // tearing down dispatch_mutex_/dispatch_drained_ (used by ~DispatchFrame).
        {
            platform::ScopedLock const dispatch_lock(dispatch_mutex_);
            dispatch_drained_.wait(dispatch_mutex_, [&] {
                return active_dispatches_ == nullptr;
            });
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
        if (!running_) {
            return false;
        }
        const platform::String<> key = make_subscription_key(service_id, instance_id, eventgroup_id);
        for (const auto& entry : subscriptions_) {
            if (entry.second.subscription.service_id == service_id && entry.first != key) {
                return false;
            }
        }
        if (subscriptions_.size() >= subscriptions_.max_size() &&
            subscriptions_.find(key) == subscriptions_.end()) {
            return false;
        }
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

        const Result send_result = transport_.send_message(subscription_msg, service_endpoint);
        bool const success = (send_result == Result::SUCCESS);
        if (!success) {
            subscriptions_.erase(key);
        }
        return success;
    }

    bool unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
        if (DispatchFrame::reentrant(*this)) {
            return remove_subscription(service_id, instance_id, eventgroup_id);
        }
        // One external waiter avoids relying on a PAL-wide broadcast operation.
        platform::ScopedLock const unsubscribe_lock(unsubscribe_mutex_);
        const platform::String<> key = make_subscription_key(service_id, instance_id, eventgroup_id);
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
        platform::ScopedLock const subs_lock(subscriptions_mutex_);
        if (running_) {
            const platform::String<> key = make_subscription_key(service_id, instance_id, eventgroup_id);

            auto it = subscriptions_.find(key);
            if (it != subscriptions_.end()) {
                found = true;

                // Best-effort: the unsubscribe message is sent when the endpoint
                // resolves, but the local subscription is erased either way so a
                // subscription whose service never resolves a port isn't stuck
                // forever (it was never reachable to notify in the first place).
                const transport::Endpoint service_endpoint =
                    resolve_service_endpoint(service_id, instance_id);
                if (service_endpoint.get_port() != 0) {
                    MessageId const msg_id(service_id, 0x0002);
                    Message unsubscription_msg(msg_id, RequestId(client_id_, 0x0002),
                                               MessageType::REQUEST, ReturnCode::E_OK);

                    // Add unsubscription data to payload
                    platform::ByteBuffer payload;
                    payload.push_back(static_cast<uint8_t>(
                        (static_cast<uint32_t>(eventgroup_id) >> 8U) & 0xFFU));
                    payload.push_back(
                        static_cast<uint8_t>(static_cast<uint32_t>(eventgroup_id) & 0xFFU));
                    unsubscription_msg.set_payload(payload);

                    const Result result = transport_.send_message(unsubscription_msg, service_endpoint);
                    if (result != Result::SUCCESS) {
                        // Log error or handle failure
                    }
                }

                subscriptions_.erase(it);
            }
        }
        if (cutoff != nullptr) {
            // Keep removal and cutoff sampling atomic with respect to resubscription.
            // Dispatch never holds dispatch_mutex_ while acquiring subscriptions_mutex_.
            platform::ScopedLock const dispatch_lock(dispatch_mutex_);
            *cutoff = next_dispatch_ticket_;
        }
        return found;
    }

   public:
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
        if (!running_) {
            return false;
        }
        const platform::String<> key = make_field_key(service_id, event_id);
        if (field_requests_.find(key) != field_requests_.end() ||
            field_requests_.size() >= field_requests_.max_size()) {
            return false;
        }
        field_requests_[key] = std::move(callback);

        MessageId const msg_id(service_id, 0x0003);
        Message field_msg(msg_id, RequestId(client_id_, 0x0003), MessageType::REQUEST,
                          ReturnCode::E_OK);

        // Add field ID to payload
        platform::ByteBuffer payload;
        payload.push_back(static_cast<uint8_t>((static_cast<uint32_t>(event_id) >> 8U) & 0xFFU));
        payload.push_back(static_cast<uint8_t>(static_cast<uint32_t>(event_id) & 0xFFU));
        field_msg.set_payload(payload);

        if (transport_.send_message(field_msg, service_endpoint) != Result::SUCCESS) {
            field_requests_.erase(key);
            return false;
        }
        return true;
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
        return running_ && transport_.is_connected();
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

    void set_message_rejection_handler(
        platform::Function<void(const transport::MessageRejectionInfo&)> handler) {
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

    // Deliberately NOT instance-scoped: a SOME/IP notification (see
    // someip::Message) carries no instance id, so on_message_received() has no
    // way to recover which instance a field response came from. Keying this on
    // instance_id would let the two call sites drift and silently stop
    // matching (see the @note on EventSubscriber::request_field).
    platform::String<> make_field_key(uint16_t service_id, uint16_t event_id) {
        platform::String<> key;
        uint16_to_str(service_id, key);
        key.append(":");
        uint16_to_str(event_id, key);
        return key;
    }

    void on_message_received(MessagePtr message, const transport::Endpoint& /*sender*/) override {
        // Check if this is an event notification
        if (message->get_message_type() != MessageType::NOTIFICATION) {
            return;
        }
        // Registered before snapshots; destroyed after local callable copies, including unwind.
        // Non-const: the matched subscription key is recorded onto it below so
        // unsubscribe_eventgroup() can scope its barrier wait to this subscription.
        DispatchFrame dispatch(*this);

        uint16_t const service_id = message->get_service_id();
        uint16_t const event_id = message->get_method_id();  // Event ID is in method ID field for notifications

        EventNotificationCallback notification_callback;
        EventNotificationCallback field_callback;
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

        {
            platform::ScopedLock const field_lock(field_requests_mutex_);
            // Not instance-scoped: see make_field_key()'s comment. notified_instance_id
            // is unused here on purpose -- the wire notification carries no instance id.
            platform::String<> const field_key = make_field_key(service_id, event_id);
            auto field_it = field_requests_.find(field_key);
            if (field_it != field_requests_.end()) {
                field_callback = std::move(field_it->second);
                field_requests_.erase(field_it);
            }
        }

        if (notification && notification_callback) {
            notification->event_data = message->get_payload();
            notification_callback(*notification);
        }
        if (field_callback) {
            EventNotification field_notification(service_id, 0, event_id);
            field_notification.event_data = message->get_payload();
            field_callback(field_notification);
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

    /** @implements REQ_TRANSPORT_026 */
    void on_message_rejected(const transport::MessageRejectionInfo& info) override {
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

    platform::UnorderedMap<platform::String<>, EventNotificationCallback> field_requests_;
    mutable platform::Mutex field_requests_mutex_;
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
static_assert(sizeof(EventSubscriberImpl) <= SOMEIP_PIMPL_EVENTSUB_SIZE,
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

EventSubscriber::~EventSubscriber() {
#ifdef SOMEIP_STATIC_ALLOC
    impl()->~EventSubscriberImpl();
#endif
}

void EventSubscriber::set_default_endpoint(const platform::String<>& address, uint16_t port) {
    impl()->set_default_endpoint(address, port);
}

void EventSubscriber::set_endpoint_resolver(EndpointResolver resolver) {
    impl()->set_endpoint_resolver(std::move(resolver));
}

bool EventSubscriber::initialize() {
    return impl()->initialize();
}

void EventSubscriber::shutdown() {
    impl()->shutdown();
}

Result EventSubscriber::get_transport_result() const
{
    return impl()->get_transport_result();
}

bool EventSubscriber::subscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                         EventNotificationCallback notification_callback,
                                         SubscriptionStatusCallback status_callback,
                                         const platform::Vector<EventFilter>& filters) {
    return impl()->subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                                     std::move(notification_callback), std::move(status_callback), filters);
}

bool EventSubscriber::unsubscribe_eventgroup(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id) {
    return impl()->unsubscribe_eventgroup(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::request_field(uint16_t service_id, uint16_t instance_id, uint16_t event_id,
                                   EventNotificationCallback callback) {
    return impl()->request_field(service_id, instance_id, event_id, std::move(callback));
}

bool EventSubscriber::set_event_filters(uint16_t service_id, uint16_t instance_id, uint16_t eventgroup_id,
                                      const platform::Vector<EventFilter>& filters) {
    return impl()->set_event_filters(service_id, instance_id, eventgroup_id, filters);
}

platform::Vector<EventSubscription> EventSubscriber::get_active_subscriptions() const {
    return impl()->get_active_subscriptions();
}

SubscriptionState EventSubscriber::get_subscription_status(uint16_t service_id, uint16_t instance_id,
                                                         uint16_t eventgroup_id) const {
    return impl()->get_subscription_status(service_id, instance_id, eventgroup_id);
}

bool EventSubscriber::is_ready() const {
    return impl()->is_ready();
}

void EventSubscriber::set_message_rejection_handler(
    platform::Function<void(const transport::MessageRejectionInfo&)> handler) {
    impl()->set_message_rejection_handler(std::move(handler));
}

EventSubscriber::Statistics EventSubscriber::get_statistics() const {
    return impl()->get_statistics();
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::events
