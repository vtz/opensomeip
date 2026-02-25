/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Minimal SOME/IP echo demo for Zephyr.
 *
 * Creates a SOME/IP message, serializes it, deserializes it, and prints
 * the result to validate that the SOME/IP core library works correctly
 * on the Zephyr target.
 */

#include <cstdio>
#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"

using namespace someip;
using namespace someip::transport;

int main() {
    printf("=== SOME/IP Echo Demo on Zephyr ===\n");

    Message msg(MessageId(0x1234, 0x0001),
                RequestId(0x0010, 0x0001),
                MessageType::REQUEST,
                ReturnCode::E_OK);

    std::vector<uint8_t> payload = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    msg.set_payload(payload);

    printf("Created message: service=0x%04X method=0x%04X\n",
           msg.get_service_id(), msg.get_method_id());
    printf("  payload size: %zu bytes\n", msg.get_payload().size());

    std::vector<uint8_t> serialized = msg.serialize();
    printf("  serialized: %zu bytes\n", serialized.size());

    Message decoded;
    if (!decoded.deserialize(serialized)) {
        printf("  deserialized FAILED\n");
        return 1;
    }

    printf("  deserialized OK: service=0x%04X method=0x%04X\n",
           decoded.get_service_id(), decoded.get_method_id());

    bool match = (decoded.get_service_id() == msg.get_service_id()) &&
                 (decoded.get_method_id() == msg.get_method_id()) &&
                 (decoded.get_payload() == msg.get_payload());
    printf("  round-trip: %s\n", match ? "PASS" : "FAIL");
    if (!match) {
        return 1;
    }

    Endpoint ep("192.168.1.100", 30490);
    printf("\nEndpoint: %s (valid=%s, ipv4=%s)\n",
           ep.to_string().c_str(),
           ep.is_valid() ? "yes" : "no",
           ep.is_ipv4() ? "yes" : "no");
    if (!ep.is_valid()) {
        return 1;
    }

    printf("=== SOME/IP Echo Demo complete ===\n");
    return 0;
}
