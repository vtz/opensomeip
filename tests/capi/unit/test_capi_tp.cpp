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
 * @file test_capi_tp.cpp
 * @brief Unit tests for C API TP (Transport Protocol) wrappers.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"
#include "static_pool_init.h"

/**
 * @test_case TC_CAPI_TP_CREATE_001
 * @tests REQ_CAPI_003, REQ_CAPI_013
 */
TEST(CapiTp, CreateAndDestroy) {
    opensomeip_tp_manager_t* tp = nullptr;
    ASSERT_EQ(opensomeip_tp_manager_create(&tp), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(tp, nullptr);
    EXPECT_EQ(opensomeip_tp_manager_destroy(tp), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_TP_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiTp, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_tp_manager_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_tp_manager_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_tp_manager_initialize(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_TP_SEGCHECK_001
 * @tests REQ_CAPI_013
 */
TEST(CapiTp, NeedsSegmentationSmallPayload) {
    opensomeip_tp_manager_t* tp = nullptr;
    ASSERT_EQ(opensomeip_tp_manager_create(&tp), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_EQ(opensomeip_tp_manager_initialize(tp), OPENSOMEIP_RESULT_SUCCESS);

    uint8_t small_payload[100];
    int needs = -1;
    EXPECT_EQ(opensomeip_tp_needs_segmentation(tp, small_payload, sizeof(small_payload), &needs),
              OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(needs, 0);

    opensomeip_tp_manager_shutdown(tp);
    opensomeip_tp_manager_destroy(tp);
}
