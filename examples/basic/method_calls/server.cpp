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
 * @brief Method Calls Server Example
 *
 * This example demonstrates basic RPC (Remote Procedure Call) functionality:
 * - Server offers a calculator service with add/multiply methods
 * - Handles method calls with parameters
 * - Returns results to clients
 *
 * This shows the fundamental RPC pattern in SOME/IP.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <unordered_map>

#include <rpc/rpc_server.h>
#include <rpc/rpc_types.h>

using namespace someip;
using namespace someip::rpc;

// Service and method IDs
const uint16_t CALCULATOR_SERVICE_ID = 0x2000;
const uint16_t ADD_METHOD_ID = 0x0001;
const uint16_t MULTIPLY_METHOD_ID = 0x0002;
const uint16_t GET_STATS_METHOD_ID = 0x0003;

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

class CalculatorServer {
public:
    CalculatorServer() : server_(CALCULATOR_SERVICE_ID) {}

    bool initialize() {
        // Register method handlers
        server_.register_method(ADD_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                     const std::vector<uint8_t>& input,
                                                     std::vector<uint8_t>& output) -> RpcResult {
            return handle_add(client_id, session_id, input, output);
        });

        server_.register_method(MULTIPLY_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                          const std::vector<uint8_t>& input,
                                                          std::vector<uint8_t>& output) -> RpcResult {
            return handle_multiply(client_id, session_id, input, output);
        });

        server_.register_method(GET_STATS_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                           const std::vector<uint8_t>& input,
                                                           std::vector<uint8_t>& output) -> RpcResult {
            return handle_get_stats(client_id, session_id, input, output);
        });

        if (!server_.initialize()) {
            std::cerr << "Failed to initialize RPC server" << std::endl;
            return false;
        }

        std::cout << "Calculator Server initialized for service 0x" << std::hex << CALCULATOR_SERVICE_ID << std::endl;
        std::cout << "Available methods:" << std::endl;
        std::cout << "  - 0x" << std::hex << ADD_METHOD_ID << ": add(int32, int32) -> int32" << std::endl;
        std::cout << "  - 0x" << std::hex << MULTIPLY_METHOD_ID << ": multiply(int32, int32) -> int32" << std::endl;
        std::cout << "  - 0x" << std::hex << GET_STATS_METHOD_ID << ": get_stats() -> struct{calls: uint32}" << std::endl;

        return true;
    }

    void run() {
        std::cout << "Calculator Server running. Press Ctrl+C to exit." << std::endl;

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        server_.shutdown();
        std::cout << "Calculator Server shut down." << std::endl;
    }

private:
    RpcServer server_;
    std::atomic<uint32_t> total_calls_{0};

    RpcResult handle_add(uint16_t client_id, uint16_t session_id,
                        const std::vector<uint8_t>& input,
                        std::vector<uint8_t>& output) {
        // Deserialize parameters
        if (input.size() < 8) {  // Need 2 int32_t parameters
            return RpcResult::INVALID_PARAMETERS;
        }

        // Extract parameters (big-endian)
        int32_t a = (input[0] << 24) | (input[1] << 16) | (input[2] << 8) | input[3];
        int32_t b = (input[4] << 24) | (input[5] << 16) | (input[6] << 8) | input[7];

        // Perform calculation
        int32_t result = a + b;
        total_calls_++;

        std::cout << "ADD: " << a << " + " << b << " = " << result
                  << " (client: 0x" << std::hex << client_id
                  << ", session: 0x" << session_id << ")" << std::endl;

        // Serialize result
        output.resize(4);
        output[0] = (result >> 24) & 0xFF;
        output[1] = (result >> 16) & 0xFF;
        output[2] = (result >> 8) & 0xFF;
        output[3] = result & 0xFF;

        return RpcResult::SUCCESS;
    }

    RpcResult handle_multiply(uint16_t client_id, uint16_t session_id,
                             const std::vector<uint8_t>& input,
                             std::vector<uint8_t>& output) {
        // Deserialize parameters
        if (input.size() < 8) {  // Need 2 int32_t parameters
            return RpcResult::INVALID_PARAMETERS;
        }

        // Extract parameters (big-endian)
        int32_t a = (input[0] << 24) | (input[1] << 16) | (input[2] << 8) | input[3];
        int32_t b = (input[4] << 24) | (input[5] << 16) | (input[6] << 8) | input[7];

        // Perform calculation
        int32_t result = a * b;
        total_calls_++;

        std::cout << "MULTIPLY: " << a << " * " << b << " = " << result
                  << " (client: 0x" << std::hex << client_id
                  << ", session: 0x" << session_id << ")" << std::endl;

        // Serialize result
        output.resize(4);
        output[0] = (result >> 24) & 0xFF;
        output[1] = (result >> 16) & 0xFF;
        output[2] = (result >> 8) & 0xFF;
        output[3] = result & 0xFF;

        return RpcResult::SUCCESS;
    }

    RpcResult handle_get_stats(uint16_t client_id, uint16_t session_id,
                              const std::vector<uint8_t>& /*input*/,
                              std::vector<uint8_t>& output) {
        uint32_t call_count = total_calls_.load();

        std::cout << "GET_STATS: " << call_count << " total calls processed"
                  << " (client: 0x" << std::hex << client_id
                  << ", session: 0x" << session_id << ")" << std::endl;

        // Serialize result (big-endian)
        output.resize(4);
        output[0] = (call_count >> 24) & 0xFF;
        output[1] = (call_count >> 16) & 0xFF;
        output[2] = (call_count >> 8) & 0xFF;
        output[3] = call_count & 0xFF;

        return RpcResult::SUCCESS;
    }
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== SOME/IP Method Calls Server ===" << std::endl;
    std::cout << std::endl;

    CalculatorServer server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    server.run();

    return 0;
}
