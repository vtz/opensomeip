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
 * @brief Hello World Client Example
 *
 * This example demonstrates the simplest possible SOME/IP client that:
 * - Sends a "Hello" message to the server
 * - Waits for and displays the response
 *
 * This is the most basic example showing core SOME/IP message exchange.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
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
std::mutex response_mutex;
std::condition_variable response_cv;
bool response_received = false;
std::string server_response;

class HelloClient : public ITransportListener {
public:
    HelloClient()
        : transport_(std::make_shared<UdpTransport>(Endpoint("0.0.0.0", 0))) {  // Client gets ephemeral port
        transport_->set_listener(this);
    }

    ~HelloClient() {
        stop();
    }

    bool start() {
        if (transport_->start() != Result::SUCCESS) {
            std::cerr << "Failed to start transport" << std::endl;
            return false;
        }

        std::cout << "Hello World Client started on " << transport_->get_local_endpoint().to_string() << std::endl;
        return true;
    }

    void stop() {
        transport_->stop();
    }

    void send_hello(const std::string& message, const std::string& host, uint16_t port) {
        // Create request message
        Message request(MessageId(HELLO_SERVICE_ID, SAY_HELLO_METHOD_ID),
                       RequestId(0x1234, 0x5678),  // Fixed client/session IDs for simplicity
                       MessageType::REQUEST,
                       ReturnCode::E_OK);

        // Set payload
        request.set_payload(std::vector<uint8_t>(message.begin(), message.end()));

        // Server endpoint
        Endpoint server_endpoint(host, port);

        std::cout << "Sending message: '" << message << "' to " << server_endpoint.to_string() << std::endl;

        // Send the message
        Result send_result = transport_->send_message(request, server_endpoint);
        if (send_result != Result::SUCCESS) {
            std::cout << "Failed to send message: " << static_cast<int>(send_result) << std::endl;
        }
    }

    void wait_for_response(int timeout_ms = 5000) {
        std::unique_lock<std::mutex> lock(response_mutex);
        if (response_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                []{ return response_received; })) {
            std::cout << "Server responded: '" << server_response << "'" << std::endl;
        } else {
            std::cout << "Timeout waiting for server response" << std::endl;
        }
    }

    // ITransportListener implementation
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::cout << "Received message from " << sender.to_string() << std::endl;
        std::cout << "Message: " << message->to_string() << std::endl;

        // Check if this is a response to our request
        if (message->get_service_id() == HELLO_SERVICE_ID &&
            message->get_method_id() == SAY_HELLO_METHOD_ID &&
            message->is_response()) {

            // Get the payload as string
            std::string response_text;
            if (!message->get_payload().empty()) {
                response_text.assign(message->get_payload().begin(), message->get_payload().end());
            }

            // Store the response and notify waiting thread
            {
                std::lock_guard<std::mutex> lock(response_mutex);
                server_response = response_text;
                response_received = true;
            }
            response_cv.notify_one();
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
    std::cout << "=== SOME/IP Hello World Client ===" << std::endl;
    std::cout << std::endl;

    HelloClient client;

    if (!client.start()) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    // Send a hello message
    // Allow overriding server host/port for cross-host testing
    auto get_env = [](const char* key, const char* def_val) {
        const char* val = std::getenv(key);
        return (val && *val) ? std::string(val) : std::string(def_val);
    };
    std::string server_host = get_env("HELLO_SERVER_HOST", "127.0.0.1");
    uint16_t server_port = 30490;
    try {
        server_port = static_cast<uint16_t>(std::stoi(get_env("HELLO_SERVER_PORT", "30490")));
    } catch (...) {
        server_port = 30490;
    }

    client.send_hello("Hello from Client!", server_host, server_port);

    // Wait for response
    client.wait_for_response();

    std::cout << "Client finished." << std::endl;
    client.stop();

    return 0;
}
