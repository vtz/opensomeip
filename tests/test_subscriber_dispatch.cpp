/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
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

// Regression tests for the PR #345 review follow-ups in EventSubscriberImpl
// (src/events/event_subscriber.cpp):
//   B1 - thread_local DispatchFrame::current_frame replaced by a PAL ThreadId.
//   B4 - unsubscribe_eventgroup() barrier must not be gated on `removed`.
//   B5 - shutdown() must drain in-flight dispatches and hand off with any
//        parked external unsubscriber before tearing down.
//   S1 - the barrier must be scoped to the specific subscription, not to a
//        global dispatch ticket.
//   S2 - make_field_key() is intentionally NOT instance-scoped (a SOME/IP
//        notification carries no instance id); this is a characterization
//        test locking in that documented behavior, not a bug fix.
//   O2 - ~EventSubscriberImpl() must not let an exception from a caller's
//        ITransport::stop() escape (it is effectively noexcept).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "events/event_subscriber.h"
#include "platform/memory.h"
#include "someip/message.h"
#include "static_pool_init.h"
#include "transport/transport.h"

namespace {

using namespace someip;
using transport::Endpoint;
using transport::ITransportListener;

constexpr uint16_t SERVICE = 0x2001;
constexpr uint16_t SERVICE_B = 0x2002;
constexpr uint16_t EVENT = 0x9001;
constexpr uint16_t GROUP_A = 0x0001;
constexpr uint16_t GROUP_B = 0x0002;
const Endpoint PEER("192.0.2.10", 32001);

// Same shape as TransportInjectionTest's FakeTransport (tests/test_transport_injection.cpp),
// trimmed to what these tests need.
class FakeTransport final : public transport::ITransport {
   public:
    Result send_message(const Message& message, const Endpoint& endpoint) override
    {
        if (!running_) {
            return Result::NOT_CONNECTED;
        }
        if (send_result != Result::SUCCESS) {
            return send_result;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        messages.push_back(message);
        destinations.push_back(endpoint);
        return Result::SUCCESS;
    }

    MessagePtr receive_message() override { return nullptr; }
    Result connect(const Endpoint&) override { return Result::NOT_IMPLEMENTED; }
    Result disconnect() override { return Result::NOT_IMPLEMENTED; }
    bool is_connected() const override { return running_; }
    Endpoint get_local_endpoint() const override { return Endpoint("192.0.2.11", 32002); }

    void set_listener(ITransportListener* listener) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listener_ = listener;
    }

    Result start() override
    {
        running_ = true;
        return start_result;
    }

    Result stop() override
    {
        running_ = remain_running_on_stop;
        if (on_stop) {
            on_stop();
        }
        return stop_result;
    }

    bool is_running() const override { return running_; }

    ITransportListener* listener()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return listener_;
    }

    bool emit(const MessagePtr& message, const Endpoint& sender = PEER)
    {
        ITransportListener* target = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_ || listener_ == nullptr) {
                return false;
            }
            target = listener_;
        }
        target->on_message_received(message, sender, get_local_endpoint());
        return true;
    }

    Result start_result{Result::SUCCESS};
    Result stop_result{Result::SUCCESS};
    Result send_result{Result::SUCCESS};
    bool remain_running_on_stop{false};
    std::vector<Message> messages;
    std::vector<Endpoint> destinations;
    std::function<void()> on_stop;

   private:
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    ITransportListener* listener_{nullptr};
};

MessagePtr make_notification(uint16_t service_id, uint16_t event_id, uint8_t payload_byte = 0)
{
    MessagePtr message = platform::allocate_message();
    if (!message) {
        return message;
    }
    *message = Message(MessageId(service_id, event_id), RequestId(0, 1), MessageType::NOTIFICATION,
                       ReturnCode::E_OK);
    message->set_payload({payload_byte});
    return message;
}

bool wait_for_subscription_count(events::EventSubscriber& subscriber, size_t expected)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        if (subscriber.get_active_subscriptions().size() == expected) {
            return true;
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

// ---------------------------------------------------------------------------
// B1: DispatchFrame reentrancy is keyed on a PAL thread id, not a thread_local
// chain. This is directly observable on host as: a call to
// unsubscribe_eventgroup() for a subscription from a DIFFERENT thread than the
// one currently running that subscription's notification callback must take
// the external (blocking) path, never the reentrant one -- even though a
// dispatch for that exact subscription is active "somewhere".
// ---------------------------------------------------------------------------
TEST(SubscriberDispatch, ReentrancyIsPerCallingThreadNotGlobal)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(1, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());

    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE, 1, GROUP_A, [&](const events::EventNotification&) {
            entered.set_value();
            released.wait();
        }));

    const auto message = make_notification(SERVICE, EVENT);
    ASSERT_NE(message, nullptr);
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    entered.get_future().wait();

    // Different thread than the one running the callback above: must be
    // treated as external and block on the barrier, not skip it as reentrant.
    auto unsubscribe = std::async(std::launch::async,
        [&] { return subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP_A); });

    EXPECT_EQ(unsubscribe.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout)
        << "a call from an unrelated thread must not be misidentified as reentrant";

    release.set_value();
    EXPECT_TRUE(dispatch.get());
    EXPECT_TRUE(unsubscribe.get());
}

// ---------------------------------------------------------------------------
// B4: the barrier must run whether or not this particular call erased the
// subscription. Reproduces "idempotent teardown": a callback reentrantly
// unsubscribes itself (erasing the entry) and then keeps using its captured
// state; a concurrent EXTERNAL unsubscribe() for the same, now-already-gone
// subscription must still wait for that callback to finish.
//
// Against the original code this call returns `false` immediately without
// waiting (bug); after the fix it blocks until the callback (and its
// DispatchFrame) is done.
// ---------------------------------------------------------------------------
TEST(SubscriberDispatch, ExternalUnsubscribeWaitsEvenWhenAlreadyRemoved)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(2, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());

    std::promise<void> erased;
    std::promise<void> release;
    auto released = release.get_future().share();
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE, 1, GROUP_A, [&](const events::EventNotification&) {
            // Reentrant self-unsubscribe: erases the subscription while this
            // DispatchFrame (and this callback's captured state) is still alive.
            EXPECT_TRUE(subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP_A));
            erased.set_value();
            released.wait();
        }));

    const auto message = make_notification(SERVICE, EVENT);
    ASSERT_NE(message, nullptr);
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    erased.get_future().wait();
    ASSERT_TRUE(wait_for_subscription_count(subscriber, 0));

    // External, idempotent second unsubscribe of the same (already-gone)
    // subscription while the original callback is still running.
    auto second_unsubscribe = std::async(std::launch::async,
        [&] { return subscriber.unsubscribe_eventgroup(SERVICE, 1, GROUP_A); });

    EXPECT_EQ(second_unsubscribe.wait_for(std::chrono::milliseconds(200)),
              std::future_status::timeout)
        << "must wait for the in-flight callback even though nothing is left to erase";

    release.set_value();
    EXPECT_TRUE(dispatch.get());
    EXPECT_FALSE(second_unsubscribe.get());  // genuinely not found this time
}

// ---------------------------------------------------------------------------
// B5: shutdown() must drain any active DispatchFrame before returning (and,
// transitively, before the destructor tears down dispatch_mutex_ /
// dispatch_drained_). Against the original code shutdown() returns while the
// callback is still executing; after the fix it blocks until the callback
// completes.
// ---------------------------------------------------------------------------
TEST(SubscriberDispatch, ShutdownDrainsInFlightDispatchBeforeReturning)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(3, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());

    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE, 1, GROUP_A, [&](const events::EventNotification&) {
            entered.set_value();
            released.wait();
        }));

    const auto message = make_notification(SERVICE, EVENT);
    ASSERT_NE(message, nullptr);
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    entered.get_future().wait();

    auto shutdown = std::async(std::launch::async, [&] { subscriber.shutdown(); });
    EXPECT_EQ(shutdown.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout)
        << "shutdown() must not return while a notification callback is still running";

    release.set_value();
    EXPECT_TRUE(dispatch.get());
    shutdown.get();
}

// ---------------------------------------------------------------------------
// S1: the barrier must be scoped to the specific subscription. Unsubscribing
// an unrelated eventgroup must not block on some other eventgroup's in-flight
// callback. Against the original code (global ticket cutoff, no key check)
// this call blocks until the unrelated callback returns; after the fix it
// completes immediately.
// ---------------------------------------------------------------------------
TEST(SubscriberDispatch, UnsubscribeIsScopedToItsOwnSubscription)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(4, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());

    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE, 1, GROUP_A, [&](const events::EventNotification&) {
            entered.set_value();
            released.wait();
        }));
    ASSERT_TRUE(subscriber.subscribe_eventgroup(
        SERVICE_B, 1, GROUP_B, [](const events::EventNotification&) {}));

    const auto message = make_notification(SERVICE, EVENT);
    ASSERT_NE(message, nullptr);
    auto dispatch = std::async(std::launch::async, [&] { return transport.emit(message); });
    entered.get_future().wait();

    // Unrelated subscription: must not wait on SERVICE/GROUP_A's callback.
    auto unrelated_unsubscribe = std::async(std::launch::async,
        [&] { return subscriber.unsubscribe_eventgroup(SERVICE_B, 1, GROUP_B); });
    EXPECT_EQ(unrelated_unsubscribe.wait_for(std::chrono::seconds(1)), std::future_status::ready)
        << "an unrelated in-flight dispatch must not block this unsubscribe";
    EXPECT_TRUE(unrelated_unsubscribe.get());

    release.set_value();
    EXPECT_TRUE(dispatch.get());
}

// ---------------------------------------------------------------------------
// S2: make_field_key() is deliberately keyed on (service_id, event_id) only,
// NOT instance_id. A SOME/IP notification (someip::Message) carries no
// instance id, so on_message_received() has no way to attribute a field
// response to one instance over another; the original hardcoded-0 key on
// both call sites was already internally consistent for that reason. This
// test locks in the documented, intentional behavior (see the @note on
// EventSubscriber::request_field) as a characterization test, so a future
// change can't silently make the two call sites drift and stop matching
// (which is exactly what naively "fixing" this to be instance_id-scoped
// does -- see TransportInjectionRouting.FieldCallbackCanRequestTheNextValue
// and .DuplicateFieldRequestPreservesTheAcceptedCallback in
// test_transport_injection.cpp, which assume this same behavior).
// ---------------------------------------------------------------------------
TEST(SubscriberDispatch, FieldRequestIsNotInstanceScoped)
{
    FakeTransport transport;
    events::EventSubscriber subscriber(5, transport);
    subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
    ASSERT_TRUE(subscriber.initialize());

    unsigned calls_instance1 = 0;
    unsigned calls_instance2 = 0;
    ASSERT_TRUE(subscriber.request_field(SERVICE, 1, EVENT,
        [&](const events::EventNotification&) { ++calls_instance1; }));
    // Same service_id/event_id, different instance_id: rejected as "already
    // pending" because the key doesn't carry instance_id.
    EXPECT_FALSE(subscriber.request_field(SERVICE, 2, EVENT,
        [&](const events::EventNotification&) { ++calls_instance2; }));

    // Whichever notification arrives first satisfies the one pending callback.
    const auto message = make_notification(SERVICE, EVENT, 0x77);
    ASSERT_NE(message, nullptr);
    ASSERT_TRUE(transport.emit(message));
    EXPECT_EQ(calls_instance1, 1u);
    EXPECT_EQ(calls_instance2, 0u);

    // The field is free again now, regardless of instance_id.
    ASSERT_TRUE(subscriber.request_field(SERVICE, 2, EVENT,
        [&](const events::EventNotification&) { ++calls_instance2; }));
}

// ---------------------------------------------------------------------------
// O2: an exception escaping the caller-supplied ITransport::stop() (reached
// through shutdown()) must not escape ~EventSubscriberImpl(). Against the
// original code (implicitly noexcept destructor, no firewall) this aborts the
// process; after the fix it is swallowed.
// ---------------------------------------------------------------------------
#ifdef __cpp_exceptions
TEST(SubscriberDispatch, DestructorDoesNotPropagateTransportStopException)
{
    FakeTransport transport;
    transport.on_stop = [] { throw std::runtime_error("boom"); };
    EXPECT_NO_THROW({
        events::EventSubscriber subscriber(7, transport);
        subscriber.set_default_endpoint(PEER.get_address(), PEER.get_port());
        ASSERT_TRUE(subscriber.initialize());
    });  // destructor runs here and must not let the exception escape
}
#endif

}  // namespace
