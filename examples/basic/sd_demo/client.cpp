/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SD Demo Client -- discovers service 0x1000 via Service Discovery,
 * then sends a few SOME/IP requests to the advertised endpoint.
 *
 * Usage:
 *   ./sd_demo_client                               # defaults: multicast 239.255.255.251:30490
 *   SD_MULTICAST=239.255.255.251 ./sd_demo_client  # override multicast group
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <vector>
#include <atomic>

#include <sd/sd_client.h>
#include <sd/sd_types.h>
#include <transport/udp_transport.h>
#include <transport/endpoint.h>
#include <someip/message.h>

using namespace someip;
using namespace someip::sd;
using namespace someip::transport;

static constexpr uint16_t SERVICE_ID   = 0x1000;
static constexpr uint16_t METHOD_HELLO = 0x0001;

static std::atomic<bool> discovered{false};
static std::string disc_address;
static uint16_t    disc_port = 0;

static volatile std::sig_atomic_t running = 1;

static void signal_handler(int) { running = 0; }

static std::string env(const char* key, const char* fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : std::string(fallback);
}

static void on_found(const std::vector<ServiceInstance>& services) {
    for (const auto& svc : services) {
        if (svc.service_id == SERVICE_ID && !svc.ip_address.empty() && svc.port != 0) {
            disc_address = svc.ip_address;
            disc_port    = svc.port;
            discovered   = true;
            std::cout << "[sd] Discovered service 0x" << std::hex << svc.service_id
                      << " at " << svc.ip_address << ":" << std::dec << svc.port
                      << std::endl;
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
        std::cerr << "[client] Send failed" << std::endl;
        return false;
    }

    for (int i = 0; i < 50; ++i) {
        auto resp = tp.receive_message();
        if (resp) {
            std::string body(resp->get_payload().begin(),
                             resp->get_payload().end());
            std::cout << "[client] Response: '" << body << "'" << std::endl;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cerr << "[client] Timeout waiting for response" << std::endl;
    return false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string sd_multicast = env("SD_MULTICAST", "239.255.255.251");

    std::cout << "=== SD Demo Client ===" << std::endl;

    SdConfig cfg;
    cfg.multicast_address = sd_multicast;
    cfg.multicast_port    = 30490;

    SdClient sd(cfg);
    if (!sd.initialize()) {
        std::cerr << "[sd] Failed to initialise SD client" << std::endl;
        return 1;
    }

    std::cout << "[sd] Finding service 0x" << std::hex << SERVICE_ID
              << std::dec << " ..." << std::endl;
    sd.find_service(SERVICE_ID, on_found, std::chrono::milliseconds(10000));

    int wait_ms = 0;
    while (!discovered && wait_ms < 15000 && running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        wait_ms += 200;
    }

    if (!discovered) {
        std::cerr << "[sd] Service not found after " << wait_ms << " ms"
                  << std::endl;
        sd.shutdown();
        return 1;
    }

    UdpTransport tp(Endpoint("0.0.0.0", 0));
    if (tp.start() != Result::SUCCESS) {
        std::cerr << "[client] Failed to start transport" << std::endl;
        sd.shutdown();
        return 1;
    }

    Endpoint server(disc_address, disc_port);
    std::cout << "[client] Sending requests to " << disc_address
              << ":" << disc_port << std::endl;

    const char* msgs[] = {"Hello via SD!", "Host client found the server", "Goodbye"};
    int ok = 0;
    for (int i = 0; i < 3 && running; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (send_and_recv(tp, server, msgs[i], static_cast<uint16_t>(i + 1)))
            ++ok;
    }

    std::cout << "\n=== Result: " << ok << "/3 round-trips OK ===" << std::endl;

    tp.stop();
    sd.shutdown();
    return (ok == 3) ? 0 : 1;
}
