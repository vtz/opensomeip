/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * UDP transport test for Zephyr.
 *
 * Creates a UDP socket, sends a SOME/IP message to localhost,
 * receives the echo, and validates the round-trip.
 * Designed for native_sim with TAP networking.
 */

#include <cstdio>
#include <cstring>
#include <chrono>
#include <atomic>

#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"
#include "transport/udp_transport.h"
#include "platform/thread.h"

using namespace someip;
using namespace someip::transport;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, name)                                   \
    do {                                                    \
        if (cond) {                                         \
            printf("  [PASS] %s\n", name);                  \
            tests_passed++;                                 \
        } else {                                            \
            printf("  [FAIL] %s\n", name);                  \
            tests_failed++;                                 \
        }                                                   \
    } while (0)

static void test_udp_loopback() {
    printf("\n--- UDP Loopback Test ---\n");

    // Bind to INADDR_ANY to avoid interface-specific bind failures on native_sim.
    Endpoint server_ep("0.0.0.0", 0);
    UdpTransportConfig config;
    // Non-blocking mode avoids native_sim shutdown hangs in receive_loop.
    config.blocking = false;

    UdpTransport server(server_ep, config);
    auto result = server.start();
    CHECK(result == Result::SUCCESS, "server_start");
    if (result != Result::SUCCESS) return;

    auto bound_ep = server.get_local_endpoint();
    Endpoint server_target("127.0.0.1", bound_ep.get_port());
    printf("  Server bound to port %d\n", bound_ep.get_port());

    Message msg(MessageId(0xABCD, 0x0001),
                RequestId(0x0001, 0x0001),
                MessageType::REQUEST,
                ReturnCode::E_OK);
    msg.set_payload({0x01, 0x02, 0x03});

    Endpoint client_ep("0.0.0.0", 0);
    UdpTransport client(client_ep, config);
    result = client.start();
    CHECK(result == Result::SUCCESS, "client_start");
    if (result != Result::SUCCESS) return;

    result = client.send_message(msg, server_target);
    CHECK(result == Result::SUCCESS, "send_message");

    MessagePtr received = nullptr;
    for (int attempt = 0; attempt < 50; ++attempt) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(10));
        received = server.receive_message();
        if (received) break;
    }
    CHECK(received != nullptr, "receive_message");

    if (received) {
        CHECK(received->get_service_id() == 0xABCD, "service_id_match");
        CHECK(received->get_payload().size() == 3, "payload_size_match");
    }

    client.stop();
    server.stop();
}

int main() {
    printf("=== SOME/IP Transport Tests on Zephyr ===\n");

    test_udp_loopback();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
