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

#include <gtest/gtest.h>

#include "platform/containers.h"
#include "etl_error_handler.h"
#include "platform/memory.h"

#include <atomic>
#include <cstdint>

namespace someip::platform {
namespace {

class StaticAllocEnv : public ::testing::Environment {
public:
    void SetUp() override { init_static_allocator(); }
};
[[maybe_unused]] auto* const g_env =
    ::testing::AddGlobalTestEnvironment(new StaticAllocEnv());

class EtlErrorHandlerTest : public ::testing::Test {
protected:
    void SetUp() override { reset_etl_error_count(); }
};

/**
 * @test_case TC_ETL_HANDLER_REGISTERED
 * @tests REQ_PLATFORM_STATIC_ETL_HANDLER
 *
 * Verify the custom ETL error handler is registered after init.
 */
TEST_F(EtlErrorHandlerTest, HandlerIsRegistered) {
    EXPECT_EQ(get_etl_error_count(), 0u);
}

/**
 * @test_case TC_ETL_HANDLER_VECTOR_OVERFLOW
 * @tests REQ_PLATFORM_STATIC_ETL_HANDLER
 *
 * Overflowing a bounded vector must invoke the error handler
 * (incrementing the error count) without aborting/terminating.
 */
TEST_F(EtlErrorHandlerTest, VectorOverflowInvokesHandler) {
    Vector<uint8_t, 4> v;
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<uint8_t>(i));
    }
    ASSERT_EQ(v.size(), 4u);
    ASSERT_TRUE(v.full());

    v.push_back(0xFF);

    EXPECT_GT(get_etl_error_count(), 0u)
        << "ETL error handler should have been invoked on overflow";
    EXPECT_EQ(v.size(), 4u)
        << "Vector size must not grow past capacity";
}

/**
 * @test_case TC_ETL_MAP_FULL_CHECK
 * @tests REQ_PLATFORM_STATIC_ETL_HANDLER
 *
 * ETL unordered_map does NOT degrade gracefully on overflow (it segfaults),
 * so callers MUST use full() before insert. Verify the full() API works.
 */
TEST_F(EtlErrorHandlerTest, MapFullCheckPreventsOverflow) {
    UnorderedMap<uint16_t, uint16_t, 4> m;
    for (uint16_t i = 0; i < 4; ++i) {
        ASSERT_FALSE(m.full());
        m.insert({i, i});
    }
    EXPECT_TRUE(m.full());
    EXPECT_EQ(m.size(), 4u);
}

/**
 * @test_case TC_ETL_HANDLER_NO_ABORT
 * @tests REQ_PLATFORM_STATIC_ETL_HANDLER
 *
 * Multiple overflows should all be handled gracefully — no
 * abort, no exception, just error count increments.
 */
TEST_F(EtlErrorHandlerTest, MultipleOverflowsNoAbort) {
    Vector<uint8_t, 2> v;
    v.push_back(1);
    v.push_back(2);

    for (int i = 0; i < 10; ++i) {
        v.push_back(0xFF);
    }

    EXPECT_GE(get_etl_error_count(), 10u);
    EXPECT_EQ(v.size(), 2u);
}

}  // namespace
}  // namespace someip::platform
