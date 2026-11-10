/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SD Demo Client -- finds the first required service from a deployment file,
 * then sends a few SOME/IP requests to the advertised endpoint.
 *
 * Usage:
 *   ./sd_demo_client
 *   ./sd_demo_client /path/sd_demo_deployment.json
 *   SD_MULTICAST=239.255.255.251 ./sd_demo_client
 *
 * The service id and SD multicast address and port come from the file.
 * SD_MULTICAST overrides the multicast address after the file is loaded.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sd/deployment.h>
#include <sd/sd_client.h>
#include <sd/sd_types.h>
#include <someip/message.h>
#include <string>
#include <thread>
#include <transport/endpoint.h>
#include <transport/udp_transport.h>
#include <vector>

using namespace someip;
using namespace someip::sd;
using namespace someip::transport;

static constexpr uint16_t METHOD_HELLO = 0x0001;

static std::atomic<bool> discovered{false};
static std::string disc_address;
static uint16_t disc_port = 0;
static uint16_t expected_service = 0;

static volatile std::sig_atomic_t running = 1;

static void signal_handler(int)
{
    running = 0;
}

static std::string env(const char* key, const char* fallback)
{
    const char* value = std::getenv(key);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : std::string(fallback);
}

static std::string deployment_path(int argc, char** argv)
{
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        return argv[1];
    }
    const std::string argv0 = (argc > 0 && argv[0] != nullptr) ? argv[0] : "";
    const auto slash = argv0.find_last_of("/\\");
    if (slash == std::string::npos) {
        return "sd_demo_deployment.json";
    }
    return argv0.substr(0, slash + 1) + "sd_demo_deployment.json";
}

static void on_found(const std::vector<ServiceInstance>& services)
{
    for (const auto& svc : services) {
        if (svc.service_id == expected_service && !svc.ip_address.empty() && svc.port != 0) {
            disc_address = svc.ip_address;
            disc_port = svc.port;
            discovered = true;
            std::cout << "[sd] Discovered service 0x" << std::hex << svc.service_id << " at "
                      << svc.ip_address << ":" << std::dec << svc.port << std::endl;
        }
    }
}

static bool send_and_recv(UdpTransport& transport, const Endpoint& server, const std::string& text,
                          uint16_t session)
{
    Message req(MessageId(expected_service, METHOD_HELLO), RequestId(0x0001, session),
                MessageType::REQUEST, ReturnCode::E_OK);
    req.set_payload(std::vector<uint8_t>(text.begin(), text.end()));

    if (transport.send_message(req, server) != Result::SUCCESS) {
        std::cerr << "[client] Send failed" << std::endl;
        return false;
    }

    for (int i = 0; i < 50; ++i) {
        auto resp = transport.receive_message();
        if (resp) {
            std::string body(resp->get_payload().begin(), resp->get_payload().end());
            std::cout << "[client] Response: '" << body << "'" << std::endl;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cerr << "[client] Timeout waiting for response" << std::endl;
    return false;
}

int main(int argc, char** argv)
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    const std::string path = deployment_path(argc, argv);
    Deployment deployment;
    if (load_deployment_file(path.c_str(), deployment) != Result::SUCCESS ||
        deployment.required_services.empty()) {
        std::cerr << "Failed to load a required service from " << path << std::endl;
        return 1;
    }
    expected_service = deployment.required_services[0].instance.service_id;

    const std::string sd_multicast = env("SD_MULTICAST", deployment.sd.multicast_address.c_str());
    SdConfig cfg = deployment.sd;
    cfg.multicast_address = sd_multicast;

    std::cout << "=== SD Demo Client ===" << std::endl;

    SdClient sd(cfg);
    if (!sd.initialize()) {
        std::cerr << "[sd] Failed to initialise SD client" << std::endl;
        return 1;
    }

    std::cout << "[sd] Finding service 0x" << std::hex << expected_service << std::dec << " ..."
              << std::endl;
    sd.find_service(expected_service, on_found, std::chrono::milliseconds(10000));

    int wait_ms = 0;
    while (!discovered && wait_ms < 15000 && running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        wait_ms += 200;
    }

    if (!discovered) {
        std::cerr << "[sd] Service not found after " << wait_ms << " ms" << std::endl;
        sd.shutdown();
        return 1;
    }

    UdpTransport transport(Endpoint("0.0.0.0", 0));
    if (transport.start() != Result::SUCCESS) {
        std::cerr << "[client] Failed to start transport" << std::endl;
        sd.shutdown();
        return 1;
    }

    Endpoint server(disc_address, disc_port);
    std::cout << "[client] Sending requests to " << disc_address << ":" << disc_port << std::endl;

    const char* msgs[] = {"Hello via SD!", "Host client found the server", "Goodbye"};
    int ok = 0;
    for (int i = 0; i < 3 && running; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (send_and_recv(transport, server, msgs[i], static_cast<uint16_t>(i + 1))) {
            ++ok;
        }
    }

    std::cout << "\n=== Result: " << ok << "/3 round-trips OK ===" << std::endl;

    transport.stop();
    sd.shutdown();
    return (ok == 3) ? 0 : 1;
}
