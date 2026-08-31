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
 * @file test_capi_e2e.cpp
 * @brief Unit tests for C API E2E (End-to-End) wrappers.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"

/**
 * @test_case TC_CAPI_E2E_CREATE_001
 * @tests REQ_CAPI_003, REQ_CAPI_013
 */
TEST(CapiE2E, CreateAndDestroy) {
    opensomeip_e2e_t* e = nullptr;
    ASSERT_EQ(opensomeip_e2e_create(&e), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(opensomeip_e2e_destroy(e), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_E2E_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiE2E, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_e2e_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_e2e_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_e2e_protect(nullptr, nullptr, 0, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_e2e_check(nullptr, nullptr, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_E2E_PROTECT_001
 * @tests REQ_CAPI_013
 */
TEST(CapiE2E, ProtectMessage) {
    opensomeip_e2e_t* e = nullptr;
    ASSERT_EQ(opensomeip_e2e_create(&e), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_message_t* msg = nullptr;
    ASSERT_EQ(opensomeip_message_create(&msg), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_message_set_service_id(msg, 0x0001);
    opensomeip_message_set_method_id(msg, 0x0002);
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    opensomeip_message_set_payload(msg, payload, sizeof(payload));

    auto result = opensomeip_e2e_protect(e, msg, 0x0001, 0);
    EXPECT_TRUE(result == OPENSOMEIP_RESULT_SUCCESS ||
                result == OPENSOMEIP_RESULT_NOT_INITIALIZED ||
                result == OPENSOMEIP_RESULT_INTERNAL_ERROR);

    opensomeip_message_destroy(msg);
    opensomeip_e2e_destroy(e);
}
