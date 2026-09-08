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
 * @brief Large Messages Client Example
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
#include <vector>
#include <iomanip>

#include <rpc/rpc_client.h>
#include <rpc/rpc_types.h>
#include <tp/tp_manager.h>
#include <transport/endpoint.h>

using namespace someip;
using namespace someip::rpc;
using namespace someip::tp;

// Service and method IDs
const uint16_t LARGE_DATA_SERVICE_ID = 0x5000;
const uint16_t SEND_LARGE_DATA_METHOD_ID = 0x0001;
const uint16_t RECEIVE_LARGE_DATA_METHOD_ID = 0x0002;
const uint16_t ECHO_LARGE_DATA_METHOD_ID = 0x0003;

class LargeMessagesClient {
public:
    LargeMessagesClient() : client_(0xABCD), tp_manager_() {}  // Client ID

    bool initialize() {
        // Initialize TP manager for large message handling
        if (!tp_manager_.initialize()) {
            std::cerr << "Failed to initialize TP manager" << std::endl;
            return false;
        }

        if (!client_.initialize()) {
            std::cerr << "Failed to initialize RPC client" << std::endl;
            return false;
        }

        client_.set_remote_endpoint(transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

        std::cout << "Large Messages Client initialized (ID: 0xABCD)" << std::endl;
        std::cout << "TP Manager configured for large message handling" << std::endl;
        return true;
    }

    void run_demonstrations() {
        std::cout << "\n=== Large Messages Demonstrations ===" << std::endl;

        // Test with different message sizes
        test_large_message_transfer(2000);   // ~2KB - requires TP segmentation
        test_large_message_transfer(10000);  // ~10KB - multiple segments
        test_large_message_transfer(50000);  // ~50KB - many segments

        std::cout << "\n=== Round-trip Echo Test ===" << std::endl;
        test_large_message_echo(15000);  // ~15KB round-trip test

        std::cout << "\n=== All Demonstrations Completed ===" << std::endl;
    }

    void shutdown() {
        tp_manager_.shutdown();
        client_.shutdown();
        std::cout << "Large Messages Client shut down." << std::endl;
    }

private:
    RpcClient client_;
    TpManager tp_manager_;

    void test_large_message_transfer(size_t message_size) {
        std::cout << "\n--- Testing " << message_size << " byte message transfer ---" << std::endl;

        // Request large data from server
        std::vector<uint8_t> request_params(4);
        request_params[0] = (message_size >> 24) & 0xFF;
        request_params[1] = (message_size >> 16) & 0xFF;
        request_params[2] = (message_size >> 8) & 0xFF;
        request_params[3] = message_size & 0xFF;

        auto start_time = std::chrono::steady_clock::now();

        RpcSyncResult result = client_.call_method_sync(
            LARGE_DATA_SERVICE_ID, SEND_LARGE_DATA_METHOD_ID, request_params);

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "❌ Failed to request large data: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        std::cout << "✓ Received " << result.return_values.size() << " bytes in " << duration.count() << "ms" << std::endl;

        // Verify the received data
        if (verify_received_data(result.return_values, message_size)) {
            std::cout << "✓ Data integrity verified" << std::endl;

            // Send the data back to server for verification
            std::cout << "Sending data back to server for verification..." << std::endl;

            RpcSyncResult verify_result = client_.call_method_sync(
                LARGE_DATA_SERVICE_ID, RECEIVE_LARGE_DATA_METHOD_ID, result.return_values);

            if (verify_result.result == RpcResult::SUCCESS && verify_result.return_values.size() >= 4) {
                uint32_t status = verify_result.return_values[0];
                uint32_t verified_size = (verify_result.return_values[1] << 16) |
                                       (verify_result.return_values[2] << 8) |
                                       verify_result.return_values[3];

                if (status == 0) {
                    std::cout << "✓ Server verified data integrity (" << verified_size << " bytes)" << std::endl;
                } else {
                    std::cout << "❌ Server detected data corruption" << std::endl;
                }
            } else {
                std::cout << "❌ Server verification failed" << std::endl;
            }
        } else {
            std::cout << "❌ Data integrity check failed" << std::endl;
        }

        // Calculate transfer statistics
        double throughput = static_cast<double>(result.return_values.size()) / duration.count() * 1000 / 1024; // KB/s
        std::cout << "Transfer rate: " << std::fixed << std::setprecision(2) << throughput << " KB/s" << std::endl;

        size_t estimated_segments = (result.return_values.size() / 1400) + 1; // MTU ~1400 payload
        std::cout << "Estimated TP segments: " << estimated_segments << std::endl;
    }

    void test_large_message_echo(size_t message_size) {
        std::cout << "\n--- Testing " << message_size << " byte round-trip echo ---" << std::endl;

        // First get data from server
        std::vector<uint8_t> request_params(4);
        request_params[0] = (message_size >> 24) & 0xFF;
        request_params[1] = (message_size >> 16) & 0xFF;
        request_params[2] = (message_size >> 8) & 0xFF;
        request_params[3] = message_size & 0xFF;

        RpcSyncResult get_result = client_.call_method_sync(
            LARGE_DATA_SERVICE_ID, SEND_LARGE_DATA_METHOD_ID, request_params);

        if (get_result.result != RpcResult::SUCCESS) {
            std::cout << "❌ Failed to get test data" << std::endl;
            return;
        }

        std::cout << "Got " << get_result.return_values.size() << " bytes of test data" << std::endl;

        // Echo the data back
        auto echo_start = std::chrono::steady_clock::now();

        RpcSyncResult echo_result = client_.call_method_sync(
            LARGE_DATA_SERVICE_ID, ECHO_LARGE_DATA_METHOD_ID, get_result.return_values);

        auto echo_end = std::chrono::steady_clock::now();
        auto echo_duration = std::chrono::duration_cast<std::chrono::milliseconds>(echo_end - echo_start);

        if (echo_result.result != RpcResult::SUCCESS) {
            std::cout << "❌ Echo failed: " << static_cast<int>(echo_result.result) << std::endl;
            return;
        }

        std::cout << "✓ Echo completed in " << echo_duration.count() << "ms" << std::endl;

        // Verify echoed data matches original
        if (echo_result.return_values.size() == get_result.return_values.size()) {
            bool data_matches = true;
            for (size_t i = 0; i < echo_result.return_values.size(); ++i) {
                if (echo_result.return_values[i] != get_result.return_values[i]) {
                    data_matches = false;
                    break;
                }
            }

            if (data_matches) {
                std::cout << "✓ Round-trip data integrity verified" << std::endl;
                double round_trip_rate = static_cast<double>(echo_result.return_values.size() * 2) /
                                       echo_duration.count() * 1000 / 1024; // KB/s (send + receive)
                std::cout << "Round-trip rate: " << std::fixed << std::setprecision(2) << round_trip_rate << " KB/s" << std::endl;
            } else {
                std::cout << "❌ Echoed data differs from original" << std::endl;
            }
        } else {
            std::cout << "❌ Echoed data size mismatch: sent " << get_result.return_values.size()
                      << ", received " << echo_result.return_values.size() << std::endl;
        }
    }

    bool verify_received_data(const std::vector<uint8_t>& data, size_t expected_size) {
        if (data.size() != expected_size) {
            std::cout << "Size mismatch: expected " << expected_size << ", got " << data.size() << std::endl;
            return false;
        }

        if (data.size() < 4) {
            std::cout << "Data too small for size marker" << std::endl;
            return false;
        }

        // Check size marker
        size_t marked_size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        if (marked_size != data.size()) {
            std::cout << "Size marker mismatch: marked " << marked_size << ", actual " << data.size() << std::endl;
            return false;
        }

        // Verify pattern (sample check - not full verification for performance)
        for (size_t i = 0; i < std::min(size_t(100), data.size()); ++i) {
            uint8_t expected = static_cast<uint8_t>(i % 256);
            if (data[i] != expected) {
                std::cout << "Pattern check failed at offset " << i << std::endl;
                return false;
            }
        }

        return true;
    }
};

int main() {
    std::cout << "=== SOME/IP Large Messages Client ===" << std::endl;
    std::cout << std::endl;

    LargeMessagesClient client;

    if (!client.initialize()) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }

    // Give server time to start
    std::cout << "Waiting for server to be ready..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    client.run_demonstrations();

    client.shutdown();

    std::cout << "\nClient finished." << std::endl;
    return 0;
}
