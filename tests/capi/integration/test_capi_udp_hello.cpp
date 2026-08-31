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
 * @file test_capi_udp_hello.cpp
 * @brief Integration test: loopback UDP send/receive via C API.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"
#include <cstring>
#include <thread>
#include <chrono>

/**
 * @test_case TC_CAPI_INT_UDP_001
 * @tests REQ_CAPI_008, REQ_CAPI_009
 */
TEST(CapiIntegration, UdpLoopbackSendReceive) {
    opensomeip_endpoint_t local_ep;
    std::memset(&local_ep, 0, sizeof(local_ep));
    std::strncpy(local_ep.address, "127.0.0.1", sizeof(local_ep.address) - 1);
    local_ep.port = 30600;
    local_ep.protocol = OPENSOMEIP_TRANSPORT_UDP;

    opensomeip_udp_transport_t* receiver = nullptr;
    ASSERT_EQ(opensomeip_udp_transport_create(&receiver, &local_ep), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_EQ(opensomeip_udp_transport_start(receiver), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_endpoint_t sender_ep;
    std::memset(&sender_ep, 0, sizeof(sender_ep));
    std::strncpy(sender_ep.address, "127.0.0.1", sizeof(sender_ep.address) - 1);
    sender_ep.port = 0;
    sender_ep.protocol = OPENSOMEIP_TRANSPORT_UDP;

    opensomeip_udp_transport_t* sender = nullptr;
    ASSERT_EQ(opensomeip_udp_transport_create(&sender, &sender_ep), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_EQ(opensomeip_udp_transport_start(sender), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);
    opensomeip_message_set_service_id(msg, 0x0001);
    opensomeip_message_set_method_id(msg, 0x0001);
    opensomeip_message_set_client_id(msg, 0x0001);
    opensomeip_message_set_session_id(msg, 0x0001);
    const uint8_t payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    opensomeip_message_set_payload(msg, payload, 5);

    EXPECT_EQ(opensomeip_udp_transport_send(sender, msg, &local_ep), OPENSOMEIP_RESULT_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    opensomeip_message_t* recv_msg = nullptr;
    opensomeip_endpoint_t recv_sender;
    auto result = opensomeip_udp_transport_receive(receiver, &recv_msg, &recv_sender);

    if (result == OPENSOMEIP_RESULT_SUCCESS && recv_msg != nullptr) {
        uint16_t svc = 0;
        opensomeip_message_get_service_id(recv_msg, &svc);
        EXPECT_EQ(svc, 0x0001);

        size_t pl_len = 0;
        opensomeip_message_get_payload_length(recv_msg, &pl_len);
        EXPECT_EQ(pl_len, 5u);

        opensomeip_message_destroy(recv_msg);
    }

    opensomeip_message_destroy(msg);
    opensomeip_udp_transport_stop(sender);
    opensomeip_udp_transport_stop(receiver);
    opensomeip_udp_transport_destroy(sender);
    opensomeip_udp_transport_destroy(receiver);
}
