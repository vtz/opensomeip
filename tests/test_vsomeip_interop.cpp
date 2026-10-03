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
 * @brief Optional live check against a vsomeip service.
 *
 * Built only with -DOPENSOMEIP_VSOMEIP_INTEROP=ON. Skips unless
 * OPENSOMEIP_VSOMEIP_SERVICE_PORT names a peer that answers SOME/IP
 * requests. The default ctest run does not build this binary.
 */

#include <gtest/gtest.h>

#include "someip/message.h"
#include "someip/types.h"
#include "transport/endpoint.h"
#include "transport/udp_transport.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

using namespace someip;
using namespace someip::transport;

namespace {

bool env_set(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

uint16_t env_u16(const char* name, uint16_t fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }
    return static_cast<uint16_t>(std::stoi(value));
}

MessagePtr wait_for_reply(UdpTransport& transport) {
    for (int i = 0; i < 100; ++i) {
        MessagePtr msg = transport.receive_message();
        if (msg) {
            return msg;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return nullptr;
}

}  // namespace

/**
 * @brief A REQUEST with protocol version 0x99 is answered by a real vsomeip service.
 *
 * Point OPENSOMEIP_VSOMEIP_SERVICE_PORT at a running vsomeip service (and
 * optionally HOST, SERVICE_ID, METHOD_ID). Without that endpoint the test
 * skips; it does not download or start vsomeip itself.
 */
TEST(VsomeipInterop, WrongProtocolVersionAgainstService) {
    if (!env_set("OPENSOMEIP_VSOMEIP_SERVICE_PORT")) {
        GTEST_SKIP() << "vsomeip peer is not available. Set OPENSOMEIP_VSOMEIP_SERVICE_PORT "
                        "to a service that answers SOME/IP requests. In-process tests cover "
                        "E_WRONG_PROTOCOL_VERSION and the 224.244.224.245:30490 SD preset.";
    }

    const uint16_t port = env_u16("OPENSOMEIP_VSOMEIP_SERVICE_PORT", 0);
    const char* host_env = std::getenv("OPENSOMEIP_VSOMEIP_SERVICE_HOST");
    const std::string host = (host_env != nullptr && host_env[0] != '\0') ? host_env : "127.0.0.1";
    const uint16_t service_id = env_u16("OPENSOMEIP_VSOMEIP_SERVICE_ID", 0x1234);
    const uint16_t method_id = env_u16("OPENSOMEIP_VSOMEIP_METHOD_ID", 0x0001);

    UdpTransport probe(Endpoint("127.0.0.1", 0));
    ASSERT_EQ(probe.start(), Result::SUCCESS);

    Message bad(MessageId(service_id, method_id), RequestId(0x1000, 0x0001), MessageType::REQUEST,
                ReturnCode::E_OK);
    bad.set_protocol_version(0x99);
    bad.set_interface_version(0x01);
    const Endpoint peer(host.c_str(), port);
    ASSERT_EQ(probe.send_message(bad, peer), Result::SUCCESS);

    MessagePtr reply = wait_for_reply(probe);
    ASSERT_NE(reply, nullptr) << "vsomeip peer did not answer the wrong-protocol REQUEST";
    EXPECT_EQ(reply->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(reply->get_return_code(), ReturnCode::E_WRONG_PROTOCOL_VERSION);
    EXPECT_EQ(reply->get_protocol_version(), SOMEIP_PROTOCOL_VERSION);
    EXPECT_EQ(reply->get_service_id(), service_id);
    EXPECT_EQ(reply->get_method_id(), method_id);
    EXPECT_EQ(reply->get_client_id(), 0x1000);

    probe.stop();
}
