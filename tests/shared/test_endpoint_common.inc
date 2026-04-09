/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared endpoint tests for RTOS and Zephyr test suites.
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 */

#ifndef SOMEIP_TEST_ENDPOINT_COMMON_INC
#define SOMEIP_TEST_ENDPOINT_COMMON_INC

#include "transport/endpoint.h"

static void test_endpoint() {
    using namespace someip::transport;

    printf("\n--- Endpoint tests ---\n");

    Endpoint ep1("192.168.1.1", 30490);
    CHECK(ep1.is_valid(), "valid_ipv4");
    CHECK(ep1.is_ipv4(), "is_ipv4");
    CHECK(!ep1.is_multicast(), "not_multicast");

    Endpoint ep2("239.118.122.69", 30490, TransportProtocol::MULTICAST_UDP);
    CHECK(ep2.is_valid(), "multicast_valid");
    CHECK(ep2.is_multicast(), "is_multicast");

    Endpoint ep3("999.999.999.999", 80);
    CHECK(!ep3.is_valid(), "invalid_ipv4");

    Endpoint ep4("", 80);
    CHECK(!ep4.is_valid(), "empty_address");

    Endpoint ep5("::1", 80);
    CHECK(ep5.is_valid(), "valid_ipv6");
    CHECK(ep5.is_ipv6(), "is_ipv6");

    // Port and address accessors
    CHECK(ep1.get_port() == 30490, "get_port");
    CHECK(ep1.get_address() == "192.168.1.1", "get_address");
}

#endif // SOMEIP_TEST_ENDPOINT_COMMON_INC
