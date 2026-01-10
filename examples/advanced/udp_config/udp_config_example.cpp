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

#include <iostream>
#include <thread>
#include <chrono>
#include <transport/udp_transport.h>
#include <transport/transport.h>
#include <someip/message.h>

using namespace someip;
using namespace someip::transport;

/**
 * @brief Simple listener for demonstration
 */
class DemoListener : public ITransportListener {
public:
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::cout << "Received message from " << sender.get_address() << ":" << sender.get_port()
                  << " - Service: 0x" << std::hex << message->get_service_id()
                  << ", Method: 0x" << message->get_method_id() << std::dec << std::endl;
    }

    void on_connection_lost(const Endpoint& endpoint) override {
        std::cout << "Connection lost to " << endpoint.get_address() << ":" << endpoint.get_port() << std::endl;
    }

    void on_connection_established(const Endpoint& endpoint) override {
        std::cout << "Connection established to " << endpoint.get_address() << ":" << endpoint.get_port() << std::endl;
    }

    void on_error(Result error) override {
        std::cout << "Transport error: " << static_cast<int>(error) << std::endl;
    }
};

/**
 * @brief Demonstrate different UDP transport configurations
 */
void demonstrate_configurations() {
    DemoListener listener;

    std::cout << "=== UDP Transport Configuration Examples ===\n" << std::endl;

    // 1. Default blocking configuration
    std::cout << "1. Default Blocking Configuration:" << std::endl;
    UdpTransport default_transport(Endpoint{"127.0.0.1", 0});
    default_transport.set_listener(&listener);
    default_transport.start();
    std::cout << "   Started on port: " << default_transport.get_local_endpoint().get_port() << std::endl;
    std::cout << "   Blocking mode: Yes (default)" << std::endl;
    default_transport.stop();
    std::cout << std::endl;

    // 2. Non-blocking configuration
    std::cout << "2. Non-Blocking Configuration:" << std::endl;
    UdpTransportConfig non_blocking_config;
    non_blocking_config.blocking = false;
    UdpTransport non_blocking_transport(Endpoint{"127.0.0.1", 0}, non_blocking_config);
    non_blocking_transport.set_listener(&listener);
    non_blocking_transport.start();
    std::cout << "   Started on port: " << non_blocking_transport.get_local_endpoint().get_port() << std::endl;
    std::cout << "   Blocking mode: No" << std::endl;
    non_blocking_transport.stop();
    std::cout << std::endl;

    // 3. High-performance configuration
    std::cout << "3. High-Performance Configuration:" << std::endl;
    UdpTransportConfig perf_config;
    perf_config.blocking = true;
    perf_config.receive_buffer_size = 262144;  // 256KB
    perf_config.send_buffer_size = 262144;     // 256KB
    perf_config.reuse_address = true;
    UdpTransport perf_transport(Endpoint{"127.0.0.1", 0}, perf_config);
    perf_transport.set_listener(&listener);
    perf_transport.start();
    std::cout << "   Started on port: " << perf_transport.get_local_endpoint().get_port() << std::endl;
    std::cout << "   Receive buffer: " << perf_config.receive_buffer_size << " bytes" << std::endl;
    std::cout << "   Send buffer: " << perf_config.send_buffer_size << " bytes" << std::endl;
    perf_transport.stop();
    std::cout << std::endl;

    // 4. Low-latency configuration
    std::cout << "4. Low-Latency Configuration:" << std::endl;
    UdpTransportConfig latency_config;
    latency_config.blocking = true;
    latency_config.receive_buffer_size = 4096;   // Small buffers for low latency
    latency_config.send_buffer_size = 4096;
    UdpTransport latency_transport(Endpoint{"127.0.0.1", 0}, latency_config);
    latency_transport.set_listener(&listener);
    latency_transport.start();
    std::cout << "   Started on port: " << latency_transport.get_local_endpoint().get_port() << std::endl;
    std::cout << "   Small buffers for minimal latency" << std::endl;
    latency_transport.stop();
    std::cout << std::endl;

    std::cout << "=== Configuration demonstration complete ===" << std::endl;
}

/**
 * @brief Demonstrate message exchange between two transports
 */
void demonstrate_message_exchange() {
    std::cout << "\n=== Message Exchange Demonstration ===\n" << std::endl;

    DemoListener listener1, listener2;

    // Create two transports
    UdpTransport transport1(Endpoint{"127.0.0.1", 0});
    UdpTransport transport2(Endpoint{"127.0.0.1", 0});

    transport1.set_listener(&listener1);
    transport2.set_listener(&listener2);

    // Start both transports
    transport1.start();
    transport2.start();

    Endpoint addr1 = transport1.get_local_endpoint();
    Endpoint addr2 = transport2.get_local_endpoint();

    std::cout << "Transport 1 listening on: " << addr1.get_address() << ":" << addr1.get_port() << std::endl;
    std::cout << "Transport 2 listening on: " << addr2.get_address() << ":" << addr2.get_port() << std::endl;

    // Create and send a message from transport1 to transport2
    Message message;
    message.set_service_id(0x1234);
    message.set_method_id(0x5678);
    message.set_client_id(0xABCD);
    message.set_session_id(0x0001);
    message.set_protocol_version(1);
    message.set_interface_version(1);
    message.set_message_type(MessageType::REQUEST);
    message.set_return_code(ReturnCode::E_OK);

    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', '!'};
    message.set_payload(payload);

    std::cout << "\nSending message from Transport 1 to Transport 2..." << std::endl;
    Result result = transport1.send_message(message, addr2);
    if (result == Result::SUCCESS) {
        std::cout << "Message sent successfully!" << std::endl;
    } else {
        std::cout << "Failed to send message: " << static_cast<int>(result) << std::endl;
    }

    // Give some time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Clean up
    transport1.stop();
    transport2.stop();

    std::cout << "=== Message exchange demonstration complete ===" << std::endl;
}

int main() {
    try {
        demonstrate_configurations();
        demonstrate_message_exchange();

        std::cout << "\nAll demonstrations completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
