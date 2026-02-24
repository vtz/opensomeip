/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SD Demo Client (Zephyr native_sim)
 *
 * Uses SdClient::find_service() to discover service 0x1000 offered by the
 * host-build sd_demo_server.  Once discovered, sends a few SOME/IP requests
 * to the advertised endpoint and prints the responses.
 */

#include <cstdio>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

#include "sd/sd_client.h"
#include "sd/sd_types.h"
#include "transport/udp_transport.h"
#include "transport/endpoint.h"
#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "platform/thread.h"

using namespace someip;
using namespace someip::sd;
using namespace someip::transport;

static constexpr uint16_t SERVICE_ID   = 0x1000;
static constexpr uint16_t METHOD_HELLO = 0x0001;

static std::atomic<bool> discovered{false};
static std::string disc_address;
static uint16_t    disc_port = 0;

static void on_found(const std::vector<ServiceInstance>& services) {
    for (auto& svc : services) {
        if (svc.service_id == SERVICE_ID && !svc.ip_address.empty() && svc.port != 0) {
            disc_address = svc.ip_address;
            disc_port    = svc.port;
            discovered   = true;
            printf("[sd] Discovered service 0x%04X at %s:%d\n",
                   svc.service_id, svc.ip_address.c_str(), svc.port);
        }
    }
}

static bool send_and_recv(UdpTransport& tp, const Endpoint& server,
                          const std::string& text, uint16_t session) {
    Message req(MessageId(SERVICE_ID, METHOD_HELLO),
                RequestId(0x0001, session),
                MessageType::REQUEST, ReturnCode::E_OK);
    req.set_payload(std::vector<uint8_t>(text.begin(), text.end()));

    if (tp.send_message(req, server) != Result::SUCCESS) {
        printf("[client] Send failed\n");
        return false;
    }

    for (int i = 0; i < 50; ++i) {
        auto resp = tp.receive_message();
        if (resp) {
            std::string body(resp->get_payload().begin(),
                             resp->get_payload().end());
            printf("[client] Response: '%s'\n", body.c_str());
            return true;
        }
        platform::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    printf("[client] Timeout waiting for response\n");
    return false;
}

int main() {
    printf("=== SOME/IP SD Client (Zephyr native_sim) ===\n");

    SdConfig cfg;
    cfg.multicast_address = "239.255.255.251";
    cfg.multicast_port    = 30490;

    SdClient sd(cfg);
    if (!sd.initialize()) {
        printf("[sd] Failed to initialise SD client\n");
        return 1;
    }

    printf("[sd] Finding service 0x%04X ...\n", SERVICE_ID);
    sd.find_service(SERVICE_ID, on_found, std::chrono::milliseconds(10000));

    int wait_ms = 0;
    while (!discovered && wait_ms < 15000) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(200));
        wait_ms += 200;
    }

    if (!discovered) {
        printf("[sd] Service not found after %d ms\n", wait_ms);
        sd.shutdown();
        return 1;
    }

    // Connect to the discovered service endpoint
    UdpTransport tp(Endpoint("0.0.0.0", 0));
    if (tp.start() != Result::SUCCESS) {
        printf("[client] Failed to start transport\n");
        sd.shutdown();
        return 1;
    }

    Endpoint server(disc_address, disc_port);
    printf("[client] Sending requests to %s:%d\n",
           disc_address.c_str(), disc_port);

    const char* msgs[] = {"Hello via SD!", "Zephyr found the host", "Goodbye"};
    int ok = 0;
    for (int i = 0; i < 3; ++i) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (send_and_recv(tp, server, msgs[i], static_cast<uint16_t>(i + 1)))
            ++ok;
    }

    printf("\n=== Result: %d/3 round-trips OK ===\n", ok);

    tp.stop();
    sd.shutdown();
    return (ok == 3) ? 0 : 1;
}
