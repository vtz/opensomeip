/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared message tests for RTOS and Zephyr test suites.
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 * Includes:  someip/message.h, someip/types.h, transport/endpoint.h
 */

#ifndef SOMEIP_TEST_MESSAGE_COMMON_INC
#define SOMEIP_TEST_MESSAGE_COMMON_INC

#include "someip/message.h"
#include "someip/types.h"
#include <vector>

static void test_message() {
    using namespace someip;

    printf("\n--- Message tests ---\n");

    Message msg(MessageId(0x1234, 0x0001),
                RequestId(0x0010, 0x0001));

    CHECK(msg.get_service_id() == 0x1234, "service_id");
    CHECK(msg.get_method_id() == 0x0001, "method_id");
    CHECK(msg.get_client_id() == 0x0010, "client_id");
    CHECK(msg.get_session_id() == 0x0001, "session_id");

    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    msg.set_payload(payload);
    CHECK(msg.get_payload().size() == 5, "payload_size");

    auto serialized = msg.serialize();
    CHECK(serialized.size() >= 16, "serialize_min_size");

    Message decoded;
    bool ok = decoded.deserialize(serialized);
    CHECK(ok, "deserialize");
    CHECK(decoded.get_service_id() == 0x1234, "roundtrip_service_id");
    CHECK(decoded.get_payload() == payload, "roundtrip_payload");

    // Message type and return code defaults
    CHECK(msg.get_message_type() == MessageType::REQUEST, "default_message_type");
    CHECK(msg.get_return_code() == ReturnCode::E_OK, "default_return_code");

    // Protocol version
    CHECK(msg.get_protocol_version() == 1, "protocol_version");
}

#endif // SOMEIP_TEST_MESSAGE_COMMON_INC
