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
 * @file test_capi_events.cpp
 * @brief Unit tests for C API event publisher/subscriber wrappers.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"

/**
 * @test_case TC_CAPI_EVT_PUB_001
 * @tests REQ_CAPI_003, REQ_CAPI_012
 */
TEST(CapiEventPublisher, CreateAndDestroy) {
    opensomeip_event_publisher_t* p = nullptr;
    ASSERT_EQ(opensomeip_event_publisher_create(&p, 0x0100, 0x0001),
              OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(opensomeip_event_publisher_destroy(p), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_EVT_PUB_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiEventPublisher, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_event_publisher_create(nullptr, 0, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_event_publisher_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_EVT_SUB_001
 * @tests REQ_CAPI_003, REQ_CAPI_012
 */
TEST(CapiEventSubscriber, CreateAndDestroy) {
    opensomeip_event_subscriber_t* s = nullptr;
    ASSERT_EQ(opensomeip_event_subscriber_create(&s, 0x0001),
              OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(opensomeip_event_subscriber_destroy(s), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_EVT_SUB_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiEventSubscriber, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_event_subscriber_create(nullptr, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_event_subscriber_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_EVT_SUB_CB_001
 * @tests REQ_CAPI_004
 */
TEST(CapiEventSubscriber, SubscribeRejectsNullCallback) {
    opensomeip_event_subscriber_t* s = nullptr;
    ASSERT_EQ(opensomeip_event_subscriber_create(&s, 0x0001), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_event_subscriber_subscribe(s, 0x01, 0x01, 0x01, nullptr, nullptr),
              OPENSOMEIP_RESULT_INVALID_ARGUMENT);

    opensomeip_event_subscriber_destroy(s);
}
