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
 * @file test_capi_sd.cpp
 * @brief Unit tests for C API service discovery wrappers.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"

/**
 * @test_case TC_CAPI_SD_CLIENT_001
 * @tests REQ_CAPI_003, REQ_CAPI_011
 */
TEST(CapiSdClient, CreateAndDestroy) {
    opensomeip_sd_client_t* c = nullptr;
    ASSERT_EQ(opensomeip_sd_client_create(&c), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(opensomeip_sd_client_destroy(c), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_SD_CLIENT_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiSdClient, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_sd_client_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_sd_client_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_SD_SERVER_001
 * @tests REQ_CAPI_003, REQ_CAPI_011
 */
TEST(CapiSdServer, CreateAndDestroy) {
    opensomeip_sd_server_t* s = nullptr;
    ASSERT_EQ(opensomeip_sd_server_create(&s), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(opensomeip_sd_server_destroy(s), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_SD_SERVER_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiSdServer, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_sd_server_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_sd_server_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_SD_FIND_NULL_001
 * @tests REQ_CAPI_004
 */
TEST(CapiSdClient, FindServiceRejectsNullCallback) {
    opensomeip_sd_client_t* c = nullptr;
    ASSERT_EQ(opensomeip_sd_client_create(&c), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_sd_client_find_service(c, 0x01, nullptr, nullptr, 1000),
              OPENSOMEIP_RESULT_INVALID_ARGUMENT);

    opensomeip_sd_client_destroy(c);
}
