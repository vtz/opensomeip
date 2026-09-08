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
 * @file test_capi_message.cpp
 * @brief Unit tests for C API message and version functions.
 *
 * All tests call only through the C API header — no C++ types.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"
#include "static_pool_init.h"
#include <cstring>

/**
 * @test_case TC_CAPI_VERSION_001
 * @tests REQ_CAPI_005
 */
TEST(CapiVersion, RuntimeVersionMatchesMacros) {
    uint32_t ver = opensomeip_capi_version();
    uint32_t expected = (OPENSOMEIP_CAPI_VERSION_MAJOR << 16) |
                        (OPENSOMEIP_CAPI_VERSION_MINOR << 8)  |
                         OPENSOMEIP_CAPI_VERSION_PATCH;
    EXPECT_EQ(ver, expected);
}

/**
 * @test_case TC_CAPI_MSG_CREATE_001
 * @tests REQ_CAPI_003, REQ_CAPI_008
 */
TEST(CapiMessage, CreateAndDestroy) {
    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(opensomeip_message_destroy(msg), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_MSG_NULL_001
 * @tests REQ_CAPI_002, REQ_CAPI_007
 */
TEST(CapiMessage, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_message_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_message_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_MSG_HEADER_001
 * @tests REQ_CAPI_008
 */
TEST(CapiMessage, HeaderFieldRoundTrip) {
    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_message_set_service_id(msg, 0x1234), OPENSOMEIP_RESULT_SUCCESS);
    uint16_t svc = 0;
    EXPECT_EQ(opensomeip_message_get_service_id(msg, &svc), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(svc, 0x1234);

    EXPECT_EQ(opensomeip_message_set_method_id(msg, 0x5678), OPENSOMEIP_RESULT_SUCCESS);
    uint16_t meth = 0;
    EXPECT_EQ(opensomeip_message_get_method_id(msg, &meth), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(meth, 0x5678);

    EXPECT_EQ(opensomeip_message_set_client_id(msg, 0xABCD), OPENSOMEIP_RESULT_SUCCESS);
    uint16_t cli = 0;
    EXPECT_EQ(opensomeip_message_get_client_id(msg, &cli), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(cli, 0xABCD);

    EXPECT_EQ(opensomeip_message_set_session_id(msg, 0x0042), OPENSOMEIP_RESULT_SUCCESS);
    uint16_t sess = 0;
    EXPECT_EQ(opensomeip_message_get_session_id(msg, &sess), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(sess, 0x0042);

    EXPECT_EQ(opensomeip_message_set_protocol_version(msg, 0x01), OPENSOMEIP_RESULT_SUCCESS);
    uint8_t pv = 0;
    EXPECT_EQ(opensomeip_message_get_protocol_version(msg, &pv), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(pv, 0x01);

    EXPECT_EQ(opensomeip_message_set_interface_version(msg, 0x02), OPENSOMEIP_RESULT_SUCCESS);
    uint8_t iv = 0;
    EXPECT_EQ(opensomeip_message_get_interface_version(msg, &iv), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(iv, 0x02);

    EXPECT_EQ(opensomeip_message_set_message_type(msg, OPENSOMEIP_MSG_REQUEST),
              OPENSOMEIP_RESULT_SUCCESS);
    opensomeip_message_type_t mt = OPENSOMEIP_MSG_RESPONSE;
    EXPECT_EQ(opensomeip_message_get_message_type(msg, &mt), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(mt, OPENSOMEIP_MSG_REQUEST);

    EXPECT_EQ(opensomeip_message_set_return_code(msg, OPENSOMEIP_RC_E_OK),
              OPENSOMEIP_RESULT_SUCCESS);
    opensomeip_return_code_t rc = OPENSOMEIP_RC_E_NOT_OK;
    EXPECT_EQ(opensomeip_message_get_return_code(msg, &rc), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(rc, OPENSOMEIP_RC_E_OK);

    opensomeip_message_destroy(msg);
}

/**
 * @test_case TC_CAPI_MSG_PAYLOAD_001
 * @tests REQ_CAPI_008
 */
TEST(CapiMessage, PayloadSetGetRoundTrip) {
    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);

    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(opensomeip_message_set_payload(msg, data, 4), OPENSOMEIP_RESULT_SUCCESS);

    size_t len = 0;
    EXPECT_EQ(opensomeip_message_get_payload_length(msg, &len), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(len, 4u);

    uint8_t buf[8];
    size_t buf_len = sizeof(buf);
    EXPECT_EQ(opensomeip_message_get_payload(msg, buf, &buf_len), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(buf_len, 4u);
    EXPECT_EQ(std::memcmp(buf, data, 4), 0);

    opensomeip_message_destroy(msg);
}

/**
 * @test_case TC_CAPI_MSG_PAYLOAD_OVERFLOW_001
 * @tests REQ_CAPI_002, REQ_CAPI_008
 */
TEST(CapiMessage, PayloadGetUndersizedBuffer) {
    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);

    const uint8_t data[] = {1, 2, 3, 4, 5};
    opensomeip_message_set_payload(msg, data, 5);

    uint8_t buf[2];
    size_t buf_len = 2;
    EXPECT_EQ(opensomeip_message_get_payload(msg, buf, &buf_len),
              OPENSOMEIP_RESULT_BUFFER_OVERFLOW);
    EXPECT_EQ(buf_len, 5u);

    opensomeip_message_destroy(msg);
}

/**
 * @test_case TC_CAPI_MSG_SERIALIZE_001
 * @tests REQ_CAPI_008
 */
TEST(CapiMessage, SerializeDeserializeRoundTrip) {
    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_message_set_service_id(msg, 0x0001);
    opensomeip_message_set_method_id(msg, 0x0002);
    opensomeip_message_set_client_id(msg, 0x0003);
    opensomeip_message_set_session_id(msg, 0x0004);
    const uint8_t payload[] = {0xCA, 0xFE};
    opensomeip_message_set_payload(msg, payload, 2);

    uint8_t wire[256];
    size_t wire_len = sizeof(wire);
    ASSERT_EQ(opensomeip_message_serialize(msg, wire, &wire_len), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_GT(wire_len, 0u);

    opensomeip_message_t* msg2 = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg2), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_EQ(opensomeip_message_deserialize(msg2, wire, wire_len), OPENSOMEIP_RESULT_SUCCESS);

    uint16_t svc = 0, meth = 0, cli = 0, sess = 0;
    opensomeip_message_get_service_id(msg2, &svc);
    opensomeip_message_get_method_id(msg2, &meth);
    opensomeip_message_get_client_id(msg2, &cli);
    opensomeip_message_get_session_id(msg2, &sess);
    EXPECT_EQ(svc, 0x0001);
    EXPECT_EQ(meth, 0x0002);
    EXPECT_EQ(cli, 0x0003);
    EXPECT_EQ(sess, 0x0004);

    uint8_t pl[8];
    size_t pl_len = sizeof(pl);
    opensomeip_message_get_payload(msg2, pl, &pl_len);
    EXPECT_EQ(pl_len, 2u);
    EXPECT_EQ(pl[0], 0xCA);
    EXPECT_EQ(pl[1], 0xFE);

    opensomeip_message_destroy(msg);
    opensomeip_message_destroy(msg2);
}

/**
 * @test_case TC_CAPI_MSG_NULL_OUT_001
 * @tests REQ_CAPI_002
 */
TEST(CapiMessage, GettersRejectNullOut) {
    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(opensomeip_message_get_service_id(msg, nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_message_get_payload_length(msg, nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    opensomeip_message_destroy(msg);
}
