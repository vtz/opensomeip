/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SOME/IP client running on Zephyr native_sim.
 *
 * Sends 3 SOME/IP REQUEST messages to the server at 127.0.0.1:30490,
 * waits for each RESPONSE, and prints the round-trip result.
 * Exits with 0 if all responses received, 1 otherwise.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"
#include "transport/udp_transport.h"
#include "platform/thread.h"

using namespace someip;
using namespace someip::transport;

static constexpr uint16_t SERVICE_ID    = 0x1000;
static constexpr uint16_t METHOD_HELLO  = 0x0001;
static constexpr uint16_t SERVER_PORT   = 30490;
static constexpr int      TIMEOUT_MS    = 3000;

static bool send_and_receive(UdpTransport& transport,
                             const Endpoint& server,
                             const std::string& text,
                             uint16_t session_id) {
    Message request(
        MessageId(SERVICE_ID, METHOD_HELLO),
        RequestId(0x0001, session_id),
        MessageType::REQUEST,
        ReturnCode::E_OK);
    request.set_payload(std::vector<uint8_t>(text.begin(), text.end()));

    printf("[client] Sending: '%s' (session 0x%04X)\n", text.c_str(), session_id);

    auto rc = transport.send_message(request, server);
    if (rc != Result::SUCCESS) {
        printf("[client] Send failed: %d\n", static_cast<int>(rc));
        return false;
    }

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(TIMEOUT_MS);

    while (std::chrono::steady_clock::now() < deadline) {
        auto reply = transport.receive_message();
        if (reply &&
            reply->get_service_id() == SERVICE_ID &&
            reply->get_method_id() == METHOD_HELLO &&
            reply->is_response()) {

            std::string body(reply->get_payload().begin(),
                             reply->get_payload().end());
            printf("[client] Response: '%s'\n", body.c_str());
            return true;
        }
        platform::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    printf("[client] Timeout waiting for response\n");
    return false;
}

int main() {
    printf("=== SOME/IP Client (Zephyr native_sim) ===\n");

    Endpoint client_ep("0.0.0.0", 0);
    UdpTransport transport(client_ep);
    if (transport.start() != Result::SUCCESS) {
        printf("[client] Failed to start transport\n");
        return 1;
    }

    auto bound = transport.get_local_endpoint();
    printf("[client] Bound to port %d\n", bound.get_port());

    Endpoint server("127.0.0.1", SERVER_PORT);

    const char* messages[] = {
        "Hello from Zephyr!",
        "SOME/IP on RTOS works",
        "Goodbye"
    };

    int ok = 0;
    for (int i = 0; i < 3; i++) {
        if (send_and_receive(transport, server, messages[i],
                             static_cast<uint16_t>(i + 1))) {
            ok++;
        }
        platform::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    transport.stop();

    printf("\n=== Result: %d/3 round-trips OK ===\n", ok);
    return (ok == 3) ? 0 : 1;
}
