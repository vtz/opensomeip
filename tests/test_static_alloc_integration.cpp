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
 * End-to-end integration tests for the static-allocation backend.
 *
 * Each test arms the malloc trap around protocol operations to prove
 * zero heap usage.  Pools are warmed up BEFORE arming so that lazy
 * first-use initialisation (if any) doesn't trip the trap.
 */

#include <gtest/gtest.h>

#include "malloc_trap.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/intrusive_ptr.h"
#include "platform/memory.h"
#include "someip/message.h"
#include "static_config.h"

#include <cstring>

namespace someip::platform {
namespace {

class StaticAllocEnv : public ::testing::Environment {
public:
    void SetUp() override { init_static_allocator(); }
};
[[maybe_unused]] auto* const g_env =
    ::testing::AddGlobalTestEnvironment(new StaticAllocEnv());

class StaticAllocIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto msg = allocate_message();
        msg.reset();

        BufferSlot* slot = acquire_buffer(64);
        if (slot) release_buffer(slot);
    }
};

/**
 * @test_case TC_STATIC_INT_ROUNDTRIP
 * @tests REQ_PAL_NOOP_HEAP_VERIFY
 * @tests REQ_PLATFORM_STATIC_002
 */
TEST_F(StaticAllocIntegrationTest, MessageSerializeDeserializeRoundTrip) {
    auto msg = allocate_message();
    ASSERT_NE(msg.get(), nullptr);

    msg->set_service_id(0x1234);
    msg->set_method_id(0x5678);

    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    msg->set_payload(payload, sizeof(payload));

    malloc_trap_arm();

    auto wire = msg->serialize();
    ASSERT_FALSE(wire.empty());

    someip::Message deserialized;
    ASSERT_TRUE(deserialized.deserialize(wire.data(), wire.size()));
    EXPECT_EQ(deserialized.get_service_id(), 0x1234);
    EXPECT_EQ(deserialized.get_method_id(), 0x5678);

    malloc_trap_disarm();
}

/**
 * @test_case TC_STATIC_INT_MSG_POOL
 * @tests REQ_PAL_NOOP_HEAP_VERIFY
 * @tests REQ_PAL_MEM_ALLOC
 */
TEST_F(StaticAllocIntegrationTest, MessagePoolAllocReleaseUnderTrap) {
    malloc_trap_arm();

    auto msg = allocate_message();
    if (msg == nullptr) {
        malloc_trap_disarm();
        FAIL() << "allocate_message() returned nullptr";
        return;
    }
    msg->set_service_id(0xABCD);
    EXPECT_EQ(msg->get_service_id(), 0xABCD);

    {
        auto copy = msg;
        EXPECT_EQ(copy->get_service_id(), 0xABCD);
    }

    msg.reset();

    malloc_trap_disarm();
}

/**
 * @test_case TC_STATIC_INT_BUFPOOL
 * @tests REQ_PAL_NOOP_HEAP_VERIFY
 * @tests REQ_PAL_BUFPOOL_ACQUIRE
 * @tests REQ_PAL_BUFPOOL_RELEASE
 */
TEST_F(StaticAllocIntegrationTest, BufferPoolOperationsUnderTrap) {
    malloc_trap_arm();

    BufferSlot* slot = acquire_buffer(64);
    if (slot == nullptr) {
        malloc_trap_disarm();
        FAIL() << "acquire_buffer(64) returned nullptr";
        return;
    }

    slot->data[0] = 0x42;
    EXPECT_EQ(slot->data[0], 0x42);

    release_buffer(slot);

    BufferSlot* reused = acquire_buffer(64);
    ASSERT_NE(reused, nullptr);
    release_buffer(reused);

    malloc_trap_disarm();
}

/**
 * @test_case TC_STATIC_INT_SERIALIZER
 * @tests REQ_PAL_NOOP_HEAP_VERIFY
 * @tests REQ_PLATFORM_STATIC_003
 */
TEST_F(StaticAllocIntegrationTest, SerializerWithStaticBuffer) {
    auto msg = allocate_message();
    ASSERT_NE(msg.get(), nullptr);

    msg->set_service_id(0x0100);
    msg->set_method_id(0x0001);
    msg->set_client_id(0x0001);
    msg->set_session_id(0x0001);

    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    msg->set_payload(payload, sizeof(payload));

    malloc_trap_arm();

    auto wire = msg->serialize();
    ASSERT_FALSE(wire.empty());
    EXPECT_GE(wire.size(), someip::Message::get_header_size() + sizeof(payload));

    malloc_trap_disarm();

    EXPECT_EQ(std::memcmp(wire.data() + someip::Message::get_header_size(),
                          payload, sizeof(payload)), 0);
}

/**
 * @test_case TC_STATIC_INT_CONTAINER_OPS
 * @tests REQ_PAL_NOOP_HEAP_VERIFY
 * @tests REQ_PAL_CONTAINER_VECTOR
 * @tests REQ_PAL_CONTAINER_STRING
 */
TEST_F(StaticAllocIntegrationTest, ContainerOperationsUnderTrap) {
    malloc_trap_arm();

    Vector<int, 8> v;
    for (int i = 0; i < 4; ++i) {
        v.push_back(i * 10);
    }
    EXPECT_EQ(v.size(), 4U);
    EXPECT_EQ(v[2], 20);

    String<32> s;
    s.append("static");
    EXPECT_EQ(s.size(), 6U);

    malloc_trap_disarm();
}

/**
 * @test_case TC_STATIC_INT_FULL_STACK
 * @tests REQ_PAL_NOOP_HEAP_VERIFY
 * @tests REQ_PLATFORM_STATIC_002
 *
 * Combined test: allocate message, populate, serialize, deserialize,
 * and use containers — all under the malloc trap.
 */
TEST_F(StaticAllocIntegrationTest, FullStackUnderTrap) {
    malloc_trap_arm();

    auto msg = allocate_message();
    if (msg == nullptr) {
        malloc_trap_disarm();
        FAIL() << "allocate_message() returned nullptr under trap";
        return;
    }

    msg->set_service_id(0xFACE);
    msg->set_method_id(0xFEED);
    msg->set_client_id(0x0001);
    msg->set_session_id(0x0042);

    const uint8_t payload[] = {0x11, 0x22, 0x33};
    msg->set_payload(payload, sizeof(payload));

    auto wire = msg->serialize();
    ASSERT_FALSE(wire.empty());

    someip::Message decoded;
    ASSERT_TRUE(decoded.deserialize(wire.data(), wire.size()));
    EXPECT_EQ(decoded.get_service_id(), 0xFACE);
    EXPECT_EQ(decoded.get_method_id(), 0xFEED);
    EXPECT_EQ(decoded.get_session_id(), 0x0042);
    EXPECT_EQ(decoded.get_payload().size(), sizeof(payload));

    msg.reset();

    malloc_trap_disarm();
}

}  // namespace
}  // namespace someip::platform
