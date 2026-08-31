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

/**
 * @file test_capi_transport.cpp
 * @brief Unit tests for C API transport wrappers.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"
#include <cstring>

/**
 * @test_case TC_CAPI_UDP_CREATE_001
 * @tests REQ_CAPI_003, REQ_CAPI_009
 */
TEST(CapiUdpTransport, CreateAndDestroy) {
    opensomeip_endpoint_t ep;
    std::memset(&ep, 0, sizeof(ep));
    std::strncpy(ep.address, "127.0.0.1", sizeof(ep.address) - 1);
    ep.port = 0;
    ep.protocol = OPENSOMEIP_TRANSPORT_UDP;

    opensomeip_udp_transport_t* t = nullptr;
    ASSERT_EQ(opensomeip_udp_transport_create(&t, &ep), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(opensomeip_udp_transport_destroy(t), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_UDP_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiUdpTransport, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_udp_transport_create(nullptr, nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_udp_transport_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_udp_transport_start(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_udp_transport_stop(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_TCP_CREATE_001
 * @tests REQ_CAPI_003, REQ_CAPI_009
 */
TEST(CapiTcpTransport, CreateAndDestroy) {
    opensomeip_tcp_transport_t* t = nullptr;
    ASSERT_EQ(opensomeip_tcp_transport_create(&t), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(opensomeip_tcp_transport_destroy(t), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_TCP_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiTcpTransport, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_tcp_transport_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_tcp_transport_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_UDP_SEND_NULL_001
 * @tests REQ_CAPI_002, REQ_CAPI_007
 */
TEST(CapiUdpTransport, SendRejectsNullArgs) {
    opensomeip_endpoint_t ep;
    std::memset(&ep, 0, sizeof(ep));
    std::strncpy(ep.address, "127.0.0.1", sizeof(ep.address) - 1);
    ep.port = 0;

    opensomeip_udp_transport_t* t = nullptr;
    ASSERT_EQ(opensomeip_udp_transport_create(&t, &ep), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_udp_transport_send(t, nullptr, &ep), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_udp_transport_send(t, nullptr, nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);

    opensomeip_udp_transport_destroy(t);
}
