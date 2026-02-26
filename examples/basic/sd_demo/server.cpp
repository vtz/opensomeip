/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SD Demo Server -- offers service 0x1000 via Service Discovery
 * and handles incoming SOME/IP requests.
 *
 * Usage:
 *   ./sd_demo_server                              # defaults: 127.0.0.1:30500
 *   SD_SERVICE_HOST=192.168.1.5 ./sd_demo_server  # advertise on a specific IP
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <string>
#include <vector>

#include <sd/sd_server.h>
#include <sd/sd_types.h>
#include <transport/udp_transport.h>
#include <transport/endpoint.h>
#include <someip/message.h>

using namespace someip;
using namespace someip::sd;
using namespace someip::transport;

static constexpr uint16_t SERVICE_ID   = 0x1000;
static constexpr uint16_t INSTANCE_ID  = 0x0001;
static constexpr uint16_t METHOD_HELLO = 0x0001;
static constexpr uint16_t SERVICE_PORT = 30500;

static volatile std::sig_atomic_t running = 1;

static void signal_handler(int) { running = 0; }

static std::string env(const char* key, const char* fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : std::string(fallback);
}

class RequestHandler : public ITransportListener {
public:
    explicit RequestHandler(std::shared_ptr<UdpTransport> tp) : transport_(tp) {}

    void on_message_received(MessagePtr msg, const Endpoint& sender) override {
        if (msg->get_service_id() != SERVICE_ID ||
            msg->get_method_id() != METHOD_HELLO || !msg->is_request())
            return;

        std::string text(msg->get_payload().begin(), msg->get_payload().end());
        std::cout << "[service] Request from " << sender.to_string()
                  << ": '" << text << "'" << std::endl;

        std::string reply = "Hello back! Got: " + text;
        Message response(MessageId(SERVICE_ID, METHOD_HELLO),
                         RequestId(msg->get_client_id(), msg->get_session_id()),
                         MessageType::RESPONSE, ReturnCode::E_OK);
        response.set_payload(std::vector<uint8_t>(reply.begin(), reply.end()));
        (void)transport_->send_message(response, sender);
    }

    void on_connection_lost(const Endpoint&) override {}
    void on_connection_established(const Endpoint&) override {}
    void on_error(Result err) override {
        std::cerr << "[service] error: " << static_cast<int>(err) << std::endl;
    }

private:
    std::shared_ptr<UdpTransport> transport_;
};

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string host = env("SD_SERVICE_HOST", "127.0.0.1");
    std::string sd_multicast = env("SD_MULTICAST", "239.255.255.251");

    // 1) Start the actual service transport on SERVICE_PORT
    auto service_transport = std::make_shared<UdpTransport>(Endpoint("0.0.0.0", SERVICE_PORT));
    RequestHandler handler(service_transport);
    service_transport->set_listener(&handler);

    if (service_transport->start() != Result::SUCCESS) {
        std::cerr << "Failed to start service transport" << std::endl;
        return 1;
    }

    std::cout << "=== SD Demo Server ===" << std::endl;
    std::cout << "[service] Listening on 0.0.0.0:" << SERVICE_PORT << std::endl;

    // 2) Start SD server and offer the service
    SdConfig sd_cfg;
    sd_cfg.multicast_address = sd_multicast;
    sd_cfg.multicast_port    = 30490;
    sd_cfg.unicast_address   = host;
    sd_cfg.cyclic_offer      = std::chrono::milliseconds(5000);

    SdServer sd(sd_cfg);
    if (!sd.initialize()) {
        std::cerr << "Failed to initialise SD server" << std::endl;
        return 1;
    }

    ServiceInstance svc(SERVICE_ID, INSTANCE_ID, 1, 0);
    svc.ttl_seconds = 60;
    std::string unicast_ep = host + ":" + std::to_string(SERVICE_PORT);
    sd.offer_service(svc, unicast_ep);

    std::cout << "[sd] Offering service 0x" << std::hex << SERVICE_ID
              << " instance 0x" << INSTANCE_ID << std::dec
              << " at " << unicast_ep << std::endl;
    std::cout << "[sd] Multicast on " << sd_multicast << ":30490" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    while (running != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down..." << std::endl;
    sd.shutdown();
    service_transport->stop();
    return 0;
}
