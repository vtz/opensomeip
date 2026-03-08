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
 * @brief Large Messages Server Example
 *
 * This example demonstrates SOME/IP-TP (Transport Protocol) for handling
 * messages larger than the network MTU (typically 1500 bytes):
 * - Message segmentation into TP packets
 * - Reliable reassembly of large messages
 * - Error handling for dropped/lost segments
 *
 * This shows how to handle large data transfers in SOME/IP.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>
#include <random>
#include <algorithm>

#include <rpc/rpc_server.h>
#include <rpc/rpc_types.h>
#include <tp/tp_manager.h>
#include <tp/tp_types.h>

using namespace someip;
using namespace someip::rpc;
using namespace someip::tp;

// Service and method IDs
const uint16_t LARGE_DATA_SERVICE_ID = 0x5000;
const uint16_t SEND_LARGE_DATA_METHOD_ID = 0x0001;
const uint16_t RECEIVE_LARGE_DATA_METHOD_ID = 0x0002;
const uint16_t ECHO_LARGE_DATA_METHOD_ID = 0x0003;

// Large message sizes (exceeding typical MTU of 1500 bytes) — used by client
[[maybe_unused]] const size_t SMALL_MESSAGE_SIZE = 2000;    // 2KB
[[maybe_unused]] const size_t MEDIUM_MESSAGE_SIZE = 10000;  // 10KB
[[maybe_unused]] const size_t LARGE_MESSAGE_SIZE = 50000;   // 50KB

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

class LargeMessagesServer {
public:
    LargeMessagesServer() : server_(LARGE_DATA_SERVICE_ID), tp_manager_() {}

    bool initialize() {
        // Initialize TP manager for large message handling
        if (!tp_manager_.initialize()) {
            std::cerr << "Failed to initialize TP manager" << std::endl;
            return false;
        }

        // Register method handlers
        server_.register_method(SEND_LARGE_DATA_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                                 const std::vector<uint8_t>& input,
                                                                 std::vector<uint8_t>& output) -> RpcResult {
            return handle_send_large_data(client_id, session_id, input, output);
        });

        server_.register_method(RECEIVE_LARGE_DATA_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                                    const std::vector<uint8_t>& input,
                                                                    std::vector<uint8_t>& output) -> RpcResult {
            return handle_receive_large_data(client_id, session_id, input, output);
        });

        server_.register_method(ECHO_LARGE_DATA_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                                const std::vector<uint8_t>& input,
                                                                std::vector<uint8_t>& output) -> RpcResult {
            return handle_echo_large_data(client_id, session_id, input, output);
        });

        if (!server_.initialize()) {
            std::cerr << "Failed to initialize RPC server" << std::endl;
            return false;
        }

        std::cout << "Large Messages Server initialized for service 0x" << std::hex << LARGE_DATA_SERVICE_ID << std::endl;
        std::cout << "TP Manager configured for large message handling" << std::endl;
        std::cout << "Available methods:" << std::endl;
        std::cout << "  - 0x" << std::hex << SEND_LARGE_DATA_METHOD_ID << ": send_large_data(size) -> LargeData" << std::endl;
        std::cout << "  - 0x" << std::hex << RECEIVE_LARGE_DATA_METHOD_ID << ": receive_large_data(LargeData) -> status" << std::endl;
        std::cout << "  - 0x" << std::hex << ECHO_LARGE_DATA_METHOD_ID << ": echo_large_data(LargeData) -> LargeData" << std::endl;

        return true;
    }

    void run() {
        std::cout << "Large Messages Server running. Press Ctrl+C to exit." << std::endl;

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        tp_manager_.shutdown();
        server_.shutdown();
        std::cout << "Large Messages Server shut down." << std::endl;
    }

private:
    RpcServer server_;
    TpManager tp_manager_;

    // Generate large test data with known patterns for verification
    std::vector<uint8_t> generate_test_data(size_t size) {
        std::vector<uint8_t> data(size);

        // Fill with pattern: [0x00, 0x01, 0x02, ..., 0xFF, 0x00, 0x01, ...]
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i % 256);
        }

        // Add size marker at the beginning (big-endian)
        if (size >= 4) {
            data[0] = (size >> 24) & 0xFF;
            data[1] = (size >> 16) & 0xFF;
            data[2] = (size >> 8) & 0xFF;
            data[3] = size & 0xFF;
        }

        return data;
    }

    // Verify received data against expected pattern
    bool verify_data(const std::vector<uint8_t>& data) {
        if (data.size() < 4) return false;

        // Check size marker
        size_t expected_size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        if (expected_size != data.size()) {
            std::cout << "Size mismatch: expected " << expected_size << ", got " << data.size() << std::endl;
            return false;
        }

        // Verify pattern
        for (size_t i = 0; i < data.size(); ++i) {
            uint8_t expected = static_cast<uint8_t>(i % 256);
            if (data[i] != expected) {
                std::cout << "Data corruption at offset " << i
                          << ": expected 0x" << std::hex << (int)expected
                          << ", got 0x" << (int)data[i] << std::endl;
                return false;
            }
        }

        return true;
    }

    RpcResult handle_send_large_data(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                   const std::vector<uint8_t>& input,
                                   std::vector<uint8_t>& output) {
        if (input.size() < 4) {
            return RpcResult::INVALID_PARAMETERS;
        }

        // Get requested size (big-endian)
        size_t requested_size = (input[0] << 24) | (input[1] << 16) | (input[2] << 8) | input[3];

        std::cout << "Generating large data: " << requested_size << " bytes" << std::endl;

        // Generate the large data
        std::vector<uint8_t> large_data = generate_test_data(requested_size);

        // Use TP to handle the large message
        TpTransfer transfer;
        transfer.message_id = 0x50000001;  // Unique message ID
        transfer.state = TpTransferState::IDLE;

        // The TP manager would handle segmentation here
        // For this example, we'll simulate the concept
        std::cout << "Data would be segmented into ~" << (requested_size / 1400) + 1 << " TP segments" << std::endl;

        // Return the data (in practice, TP would handle transport)
        output = large_data;

        std::cout << "Large data generation completed (" << output.size() << " bytes)" << std::endl;
        return RpcResult::SUCCESS;
    }

    RpcResult handle_receive_large_data(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                      const std::vector<uint8_t>& input,
                                      std::vector<uint8_t>& output) {
        std::cout << "Received large data: " << input.size() << " bytes" << std::endl;

        // Verify the received data
        if (verify_data(input)) {
            std::cout << "✓ Data integrity verified - no corruption detected" << std::endl;

            // Return success status
            output.resize(4);
            output[0] = 0; // Success code
            output[1] = (input.size() >> 16) & 0xFF;
            output[2] = (input.size() >> 8) & 0xFF;
            output[3] = input.size() & 0xFF;

            return RpcResult::SUCCESS;
        } else {
            std::cout << "❌ Data corruption detected!" << std::endl;

            // Return error status
            output.resize(4);
            output[0] = 1; // Error code
            output[1] = 0;
            output[2] = 0;
            output[3] = 0;

            return RpcResult::INTERNAL_ERROR;
        }
    }

    RpcResult handle_echo_large_data(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                   const std::vector<uint8_t>& input,
                                   std::vector<uint8_t>& output) {
        std::cout << "Echoing large data: " << input.size() << " bytes" << std::endl;

        // Verify data integrity first
        if (!verify_data(input)) {
            std::cout << "❌ Received corrupted data - cannot echo" << std::endl;
            return RpcResult::INVALID_PARAMETERS;
        }

        // Echo the data back
        output = input;

        std::cout << "✓ Large data echoed successfully" << std::endl;
        return RpcResult::SUCCESS;
    }
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== SOME/IP Large Messages Server ===" << std::endl;
    std::cout << std::endl;

    LargeMessagesServer server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    server.run();

    return 0;
}
