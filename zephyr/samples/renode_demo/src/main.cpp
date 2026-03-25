/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Renode demo: SOME/IP UDP request/response on simulated S32K388.
 *
 * Demonstrates the full SOME/IP message lifecycle on a Renode-simulated
 * Cortex-M7 target with Zephyr networking:
 *
 *  1. Server thread binds a UDP socket and waits for a request
 *  2. Client thread sends a SOME/IP REQUEST with a payload
 *  3. Server deserializes the request, builds a RESPONSE, and sends it back
 *  4. Client receives and validates the response
 *
 * Output goes to UART (LPUART0) so Renode's FileTerminal can capture it
 * for automated validation.
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

static const uint16_t CLIENT_PORT = 30501;
static std::atomic<uint16_t> server_port{0};
static std::atomic<bool> server_ready{false};

static std::atomic<int> demo_passed{0};
static std::atomic<int> demo_failed{0};

#define DEMO_CHECK(cond, name)                                  \
    do {                                                        \
        if (cond) {                                             \
            printf("  [PASS] %s\n", name);                      \
            demo_passed.fetch_add(1);                           \
        } else {                                                \
            printf("  [FAIL] %s\n", name);                      \
            demo_failed.fetch_add(1);                           \
        }                                                       \
    } while (0)

static void server_task() {
    printf("\n--- Server: starting ---\n");

    Endpoint bind_ep("0.0.0.0", 0);
    UdpTransportConfig config;
    config.blocking = false;

    UdpTransport server(bind_ep, config);
    auto rc = server.start();
    if (rc != Result::SUCCESS) {
        printf("  Server: bind failed\n");
        return;
    }

    auto local = server.get_local_endpoint();
    server_port.store(local.get_port());
    printf("  Server: listening on port %d\n", local.get_port());
    server_ready.store(true);

    MessagePtr request = nullptr;
    for (int i = 0; i < 200 && !request; ++i) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(10));
        request = server.receive_message();
    }

    DEMO_CHECK(request != nullptr, "server_received_request");
    if (!request) {
        server.stop();
        return;
    }

    printf("  Server: received request service=0x%04X method=0x%04X payload=%zu bytes\n",
           request->get_service_id(), request->get_method_id(),
           request->get_payload().size());

    DEMO_CHECK(request->get_service_id() == 0x4711, "server_service_id");
    DEMO_CHECK(request->get_method_id() == 0x0001, "server_method_id");

    Message response(
        MessageId(request->get_service_id(), request->get_method_id()),
        RequestId(request->get_client_id(), request->get_session_id()),
        MessageType::RESPONSE,
        ReturnCode::E_OK);

    std::vector<uint8_t> resp_payload = {0xCA, 0xFE};
    response.set_payload(resp_payload);

    Endpoint client_addr("127.0.0.1", CLIENT_PORT);
    rc = server.send_message(response, client_addr);
    printf("  Server: sent response (%s)\n",
           rc == Result::SUCCESS ? "OK" : "FAILED");

    server.stop();
    printf("--- Server: done ---\n");
}

static void client_task() {
    while (!server_ready.load()) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    printf("\n--- Client: starting ---\n");

    Endpoint bind_ep("0.0.0.0", CLIENT_PORT);
    UdpTransportConfig config;
    config.blocking = false;

    UdpTransport client(bind_ep, config);
    auto rc = client.start();
    DEMO_CHECK(rc == Result::SUCCESS, "client_start");
    if (rc != Result::SUCCESS) return;

    Message request(
        MessageId(0x4711, 0x0001),
        RequestId(0x0001, 0x0001),
        MessageType::REQUEST,
        ReturnCode::E_OK);

    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    request.set_payload(payload);

    Endpoint server_ep("127.0.0.1", server_port.load());
    rc = client.send_message(request, server_ep);
    DEMO_CHECK(rc == Result::SUCCESS, "client_send_request");

    printf("  Client: sent request to port %d (%zu bytes payload)\n",
           server_port.load(), payload.size());

    MessagePtr response = nullptr;
    for (int i = 0; i < 200 && !response; ++i) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(10));
        response = client.receive_message();
    }

    DEMO_CHECK(response != nullptr, "client_received_response");
    if (response) {
        DEMO_CHECK(response->get_service_id() == 0x4711, "client_response_service_id");
        DEMO_CHECK(response->get_payload().size() == 2, "client_response_payload_size");
        printf("  Client: response service=0x%04X payload=%zu bytes\n",
               response->get_service_id(), response->get_payload().size());
    }

    client.stop();
    printf("--- Client: done ---\n");
}

int main() {
    printf("=== SOME/IP Renode Demo: UDP Request/Response ===\n");

    platform::Thread srv(server_task);
    platform::Thread cli(client_task);

    cli.join();
    srv.join();

    printf("\n=== Results: %d passed, %d failed ===\n",
           demo_passed.load(), demo_failed.load());
    printf("=== Demo complete ===\n");

    return demo_failed.load() > 0 ? 1 : 0;
}
