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
 * @file test_capi_rpc_roundtrip.cpp
 * @brief Integration test: RPC client/server round-trip via C API.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"

/**
 * @test_case TC_CAPI_INT_RPC_001
 * @tests REQ_CAPI_004, REQ_CAPI_010
 */
TEST(CapiIntegration, RpcClientServerCreateDestroy) {
    opensomeip_rpc_server_t* server = nullptr;
    ASSERT_EQ(opensomeip_rpc_server_create(&server, 0x0100), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_rpc_client_t* client = nullptr;
    ASSERT_EQ(opensomeip_rpc_client_create(&client, 0x0001), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_rpc_client_destroy(client);
    opensomeip_rpc_server_destroy(server);
}

static opensomeip_result_t echo_handler(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                         const uint8_t* input_data, size_t input_len,
                                         uint8_t* output_data, size_t* output_len,
                                         void* /*user_data*/) {
    if (*output_len < input_len) {
        *output_len = input_len;
        return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
    }
    if (input_data && input_len > 0 && output_data) {
        std::memcpy(output_data, input_data, input_len);
    }
    *output_len = input_len;
    return OPENSOMEIP_RESULT_SUCCESS;
}

/**
 * @test_case TC_CAPI_INT_RPC_002
 * @tests REQ_CAPI_004, REQ_CAPI_010
 */
TEST(CapiIntegration, RpcServerRegisterHandler) {
    opensomeip_rpc_server_t* server = nullptr;
    ASSERT_EQ(opensomeip_rpc_server_create(&server, 0x0100), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_EQ(opensomeip_rpc_server_initialize(server), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_rpc_server_register_method(server, 0x0001, echo_handler, nullptr),
              OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_rpc_server_unregister_method(server, 0x0001),
              OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_rpc_server_shutdown(server);
    opensomeip_rpc_server_destroy(server);
}
