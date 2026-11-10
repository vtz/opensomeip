/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SD Demo Server -- offers the first provided service from a deployment file
 * and handles incoming SOME/IP requests.
 *
 * Usage:
 *   ./sd_demo_server
 *   ./sd_demo_server /path/sd_demo_deployment.json
 *   SD_SERVICE_HOST=192.168.1.5 ./sd_demo_server
 *   SD_MULTICAST=239.255.255.251 ./sd_demo_server
 *
 * Service id, instance, unreliable port, eventgroups, and SD timing come from
 * the file. The host and multicast environment variables override those two
 * addresses after the file is loaded. This demo's transport is UDP, so the
 * provided service needs a non-zero unreliable_port.
 */

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sd/deployment.h>
#include <sd/sd_server.h>
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

class RequestHandler : public ITransportListener {
   public:
    RequestHandler(std::shared_ptr<UdpTransport> transport, uint16_t service_id)
        : transport_(std::move(transport)), service_id_(service_id)
    {
    }

    void on_message_received(MessagePtr msg, const Endpoint& sender) override
    {
        if (msg->get_service_id() != service_id_ || msg->get_method_id() != METHOD_HELLO ||
            !msg->is_request()) {
            return;
        }

        std::string text(msg->get_payload().begin(), msg->get_payload().end());
        std::cout << "[service] Request from " << sender.to_string() << ": '" << text << "'"
                  << std::endl;

        std::string reply = "Hello back! Got: " + text;
        Message response(MessageId(service_id_, METHOD_HELLO),
                         RequestId(msg->get_client_id(), msg->get_session_id()),
                         MessageType::RESPONSE, ReturnCode::E_OK);
        response.set_payload(std::vector<uint8_t>(reply.begin(), reply.end()));
        (void)transport_->send_message(response, sender);
    }

    void on_connection_lost(const Endpoint&) override
    {
    }
    void on_connection_established(const Endpoint&) override
    {
    }
    void on_error(Result err) override
    {
        std::cerr << "[service] error: " << static_cast<int>(err) << std::endl;
    }

   private:
    std::shared_ptr<UdpTransport> transport_;
    uint16_t service_id_;
};

int main(int argc, char** argv)
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    const std::string path = deployment_path(argc, argv);
    Deployment deployment;
    if (load_deployment_file(path.c_str(), deployment) != Result::SUCCESS) {
        std::cerr << "Failed to load deployment " << path << std::endl;
        return 1;
    }
    if (deployment.provided_services.empty() ||
        deployment.provided_services[0].unreliable_port == 0) {
        std::cerr << "Deployment needs a provided service with an unreliable port" << std::endl;
        return 1;
    }

    const DeployedService& offered = deployment.provided_services[0];
    SdConfig sd_cfg = deployment.sd;
    const std::string host = env("SD_SERVICE_HOST", sd_cfg.unicast_address.c_str());
    const std::string sd_multicast = env("SD_MULTICAST", sd_cfg.multicast_address.c_str());
    sd_cfg.unicast_address = host;
    sd_cfg.multicast_address = sd_multicast;
    const uint16_t service_port = offered.unreliable_port;

    auto service_transport = std::make_shared<UdpTransport>(Endpoint("0.0.0.0", service_port));
    RequestHandler handler(service_transport, offered.instance.service_id);
    service_transport->set_listener(&handler);

    if (service_transport->start() != Result::SUCCESS) {
        std::cerr << "Failed to start service transport" << std::endl;
        return 1;
    }

    std::cout << "=== SD Demo Server ===" << std::endl;
    std::cout << "[service] Listening on 0.0.0.0:" << service_port << std::endl;

    SdServer sd(sd_cfg);
    if (!sd.initialize()) {
        std::cerr << "Failed to initialise SD server" << std::endl;
        return 1;
    }

    ServiceInstance svc = offered.instance;
    svc.ip_address = host;
    const std::string unicast_ep = host + ":" + std::to_string(service_port);
    sd.offer_service(svc, unicast_ep, "", offered.eventgroups);

    std::cout << "[sd] Offering service 0x" << std::hex << svc.service_id << " instance 0x"
              << svc.instance_id << std::dec << " at " << unicast_ep << std::endl;
    std::cout << "[sd] Multicast on " << sd_multicast << ":" << sd_cfg.multicast_port << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    while (running != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down..." << std::endl;
    sd.shutdown();
    service_transport->stop();
    return 0;
}
