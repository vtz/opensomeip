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
 * @file test_capi_rpc.cpp
 * @brief Unit tests for C API RPC client/server wrappers.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"

/**
 * @test_case TC_CAPI_RPC_CLIENT_001
 * @tests REQ_CAPI_003, REQ_CAPI_010
 */
TEST(CapiRpcClient, CreateAndDestroy) {
    opensomeip_rpc_client_t* c = nullptr;
    ASSERT_EQ(opensomeip_rpc_client_create(&c, 0x0001), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(opensomeip_rpc_client_destroy(c), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_RPC_CLIENT_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiRpcClient, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_rpc_client_create(nullptr, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_rpc_client_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_rpc_client_initialize(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_rpc_client_shutdown(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_RPC_SERVER_001
 * @tests REQ_CAPI_003, REQ_CAPI_010
 */
TEST(CapiRpcServer, CreateAndDestroy) {
    opensomeip_rpc_server_t* s = nullptr;
    ASSERT_EQ(opensomeip_rpc_server_create(&s, 0x0100), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(opensomeip_rpc_server_destroy(s), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_RPC_SERVER_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiRpcServer, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_rpc_server_create(nullptr, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_rpc_server_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_RPC_ASYNC_NULL_001
 * @tests REQ_CAPI_004
 */
TEST(CapiRpcClient, AsyncCallRejectsNullCallback) {
    opensomeip_rpc_client_t* c = nullptr;
    ASSERT_EQ(opensomeip_rpc_client_create(&c, 0x0001), OPENSOMEIP_RESULT_SUCCESS);

    uint32_t handle = 0;
    EXPECT_EQ(opensomeip_rpc_client_call_async(c, 0x01, 0x01, nullptr, 0,
                                                nullptr, nullptr, 1000, &handle),
              OPENSOMEIP_RESULT_INVALID_ARGUMENT);

    opensomeip_rpc_client_destroy(c);
}

/**
 * @test_case TC_CAPI_RPC_REGISTER_NULL_001
 * @tests REQ_CAPI_004
 */
TEST(CapiRpcServer, RegisterRejectsNullHandler) {
    opensomeip_rpc_server_t* s = nullptr;
    ASSERT_EQ(opensomeip_rpc_server_create(&s, 0x0100), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_rpc_server_register_method(s, 0x01, nullptr, nullptr),
              OPENSOMEIP_RESULT_INVALID_ARGUMENT);

    opensomeip_rpc_server_destroy(s);
}
