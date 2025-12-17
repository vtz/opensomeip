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
 * @brief Hello World Server Example
 *
 * This example demonstrates the simplest possible SOME/IP server that:
 * - Listens for incoming messages
 * - Responds with a greeting when receiving "Hello"
 *
 * This is the most basic example showing core SOME/IP message handling.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <cstdlib>
#include <string>

#include <transport/udp_transport.h>
#include <transport/endpoint.h>
#include <someip/message.h>

using namespace someip;
using namespace someip::transport;

// Service and method IDs for the Hello World service
const uint16_t HELLO_SERVICE_ID = 0x1000;
const uint16_t SAY_HELLO_METHOD_ID = 0x0001;

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

class HelloServer : public ITransportListener {
public:
    HelloServer(const std::string& host, uint16_t port)
        : transport_(std::make_shared<UdpTransport>(Endpoint(host, port))) {
        transport_->set_listener(this);
    }

    ~HelloServer() {
        stop();
    }

    bool start() {
        if (transport_->start() != Result::SUCCESS) {
            std::cerr << "Failed to start transport" << std::endl;
            return false;
        }

        std::cout << "Hello World Server started on " << transport_->get_local_endpoint().to_string() << std::endl;
        std::cout << "Waiting for 'Hello' messages..." << std::endl;
        return true;
    }

    void stop() {
        transport_->stop();
    }

    // ITransportListener implementation
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::cout << "Received message from " << sender.to_string() << std::endl;
        std::cout << "Message: " << message->to_string() << std::endl;

        // Check if this is a request to our service and method
        if (message->get_service_id() == HELLO_SERVICE_ID &&
            message->get_method_id() == SAY_HELLO_METHOD_ID &&
            message->is_request()) {

            // Get the payload as string
            std::string received_text;
            if (!message->get_payload().empty()) {
                received_text.assign(message->get_payload().begin(), message->get_payload().end());
            }

            std::cout << "Client said: '" << received_text << "'" << std::endl;

            // Create response message
            Message response(MessageId(HELLO_SERVICE_ID, SAY_HELLO_METHOD_ID),
                           RequestId(message->get_client_id(), message->get_session_id()),
                           MessageType::RESPONSE,
                           ReturnCode::E_OK);

            // Set response payload
            std::string greeting = "Hello World! Server received: " + received_text;
            response.set_payload(std::vector<uint8_t>(greeting.begin(), greeting.end()));

            // Send response
            Result send_result = transport_->send_message(response, sender);
            if (send_result == Result::SUCCESS) {
                std::cout << "Sent greeting: '" << greeting << "'" << std::endl;
            } else {
                std::cout << "Failed to send response: " << static_cast<int>(send_result) << std::endl;
            }
        }
    }

    void on_connection_lost(const Endpoint& endpoint) override {
        std::cout << "Connection lost to " << endpoint.to_string() << std::endl;
    }

    void on_connection_established(const Endpoint& endpoint) override {
        std::cout << "Connection established to " << endpoint.to_string() << std::endl;
    }

    void on_error(Result error) override {
        std::cout << "Transport error: " << static_cast<int>(error) << std::endl;
    }

private:
    std::shared_ptr<UdpTransport> transport_;
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Bind host/port can be overridden for container or cross-host testing
    auto get_env = [](const char* key, const char* def_val) {
        const char* val = std::getenv(key);
        return (val && *val) ? std::string(val) : std::string(def_val);
    };
    std::string bind_host = get_env("HELLO_BIND_HOST", "0.0.0.0");
    uint16_t bind_port = 30490;
    try {
        bind_port = static_cast<uint16_t>(std::stoi(get_env("HELLO_BIND_PORT", "30490")));
    } catch (...) {
        bind_port = 30490;
    }

    HelloServer server(bind_host, bind_port);

    std::cout << "=== SOME/IP Hello World Server ===" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << std::endl;

    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down server..." << std::endl;
    server.stop();

    std::cout << "Server stopped." << std::endl;
    return 0;
}
