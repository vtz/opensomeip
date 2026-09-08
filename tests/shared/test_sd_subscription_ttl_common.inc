/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared SD subscription TTL tests for host and Zephyr test suites.
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 * Requires:  events/event_publisher.h  and its dependencies linkable.
 *
 * Tests that EventPublisher enforces subscription TTL, the core fix
 * for issue #266.
 */

#ifndef SOMEIP_TEST_SD_SUBSCRIPTION_TTL_COMMON_INC
#define SOMEIP_TEST_SD_SUBSCRIPTION_TTL_COMMON_INC

#include "events/event_publisher.h"
#include "events/event_types.h"

static void test_sd_subscription_ttl() {
    using namespace someip::events;

    printf("\n--- SD Subscription TTL tests ---\n");

    {
        EventPublisher pub(0x1234, 0x0001);
        pub.set_default_client_endpoint("127.0.0.1", 50000);

        bool ok = pub.handle_subscription(0x0001, 0x0100, 3600u);
        CHECK(ok, "subscribe_with_ttl_stored");

        auto subs = pub.get_subscriptions(0x0001);
        CHECK(subs.size() == 1, "subscription_count_is_one");
        CHECK(subs[0] == 0x0100, "subscriber_id_matches");
    }

    {
        EventPublisher pub(0x1234, 0x0001);
        pub.set_default_client_endpoint("127.0.0.1", 50000);

        pub.handle_subscription(0x0001, 0x0100, 3600u);

        pub.handle_subscription(0x0001, 0x0100, 0u);
        auto subs = pub.get_subscriptions(0x0001);
        CHECK(subs.empty(), "stop_subscribe_ttl_zero_removes");
    }

    {
        EventPublisher pub(0x1234, 0x0001);
        pub.set_default_client_endpoint("127.0.0.1", 50000);

        pub.handle_subscription(0x0001, 0x0100, 0xFFFFFFu);
        pub.cleanup_expired_subscriptions();
        auto subs = pub.get_subscriptions(0x0001);
        CHECK(subs.size() == 1, "infinite_ttl_never_expires");
    }

    {
        EventPublisher pub(0x1234, 0x0001);
        pub.set_default_client_endpoint("127.0.0.1", 50000);

        pub.handle_subscription(0x0001, 0x0100);
        pub.cleanup_expired_subscriptions();
        auto subs = pub.get_subscriptions(0x0001);
        CHECK(subs.size() == 1, "default_no_ttl_is_infinite");
    }

    {
        EventPublisher pub(0x1234, 0x0001);
        pub.set_default_client_endpoint("127.0.0.1", 50000);

        bool result = pub.handle_subscription(0x0001, 0x0100, 0u);
        CHECK(!result, "stop_subscribe_nonexistent_returns_false");
    }
}

#endif // SOMEIP_TEST_SD_SUBSCRIPTION_TTL_COMMON_INC
