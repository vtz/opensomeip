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
 * @brief Method Calls Client Example
 *
 * This example demonstrates basic RPC (Remote Procedure Call) functionality:
 * - Client makes synchronous method calls to server
 * - Passes parameters and receives results
 * - Handles different return codes and errors
 *
 * This shows the fundamental RPC client patterns in SOME/IP.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#include <rpc/rpc_client.h>
#include <rpc/rpc_types.h>
#include <transport/endpoint.h>

using namespace someip;
using namespace someip::rpc;

// Service and method IDs
const uint16_t CALCULATOR_SERVICE_ID = 0x2000;
const uint16_t ADD_METHOD_ID = 0x0001;
const uint16_t MULTIPLY_METHOD_ID = 0x0002;
const uint16_t GET_STATS_METHOD_ID = 0x0003;

class CalculatorClient {
public:
    CalculatorClient() : client_(0xABCD) {}  // Client ID

    bool initialize() {
        if (!client_.initialize()) {
            std::cerr << "Failed to initialize RPC client" << std::endl;
            return false;
        }

        client_.set_remote_endpoint(transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

        std::cout << "Calculator Client initialized (ID: 0xABCD)" << std::endl;
        return true;
    }

    void run_calculations() {
        std::cout << "\n=== Running Calculator Operations ===" << std::endl;

        // Test ADD operation
        test_add(10, 5);
        test_add(-3, 7);
        test_add(1000, 2000);

        // Test MULTIPLY operation
        test_multiply(6, 7);
        test_multiply(-4, 5);
        test_multiply(25, 4);

        // Test GET_STATS operation
        test_get_stats();

        std::cout << "\n=== All Operations Completed ===" << std::endl;
    }

    void shutdown() {
        client_.shutdown();
        std::cout << "Calculator Client shut down." << std::endl;
    }

private:
    RpcClient client_;

    void test_add(int32_t a, int32_t b) {
        std::cout << "\n--- Testing ADD(" << a << ", " << b << ") ---" << std::endl;

        // Serialize parameters (big-endian)
        std::vector<uint8_t> parameters(8);  // 2 int32_t parameters
        parameters[0] = (a >> 24) & 0xFF;
        parameters[1] = (a >> 16) & 0xFF;
        parameters[2] = (a >> 8) & 0xFF;
        parameters[3] = a & 0xFF;
        parameters[4] = (b >> 24) & 0xFF;
        parameters[5] = (b >> 16) & 0xFF;
        parameters[6] = (b >> 8) & 0xFF;
        parameters[7] = b & 0xFF;

        // Make synchronous RPC call
        RpcSyncResult result = client_.call_method_sync(
            CALCULATOR_SERVICE_ID, ADD_METHOD_ID, parameters);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "RPC call failed: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        // Deserialize result
        if (result.return_values.size() < 4) {
            std::cout << "Invalid response size" << std::endl;
            return;
        }

        int32_t sum = (result.return_values[0] << 24) |
                     (result.return_values[1] << 16) |
                     (result.return_values[2] << 8) |
                     result.return_values[3];

        std::cout << "Result: " << a << " + " << b << " = " << sum << std::endl;
        std::cout << "✓ ADD operation successful" << std::endl;
    }

    void test_multiply(int32_t a, int32_t b) {
        std::cout << "\n--- Testing MULTIPLY(" << a << ", " << b << ") ---" << std::endl;

        // Serialize parameters (big-endian)
        std::vector<uint8_t> parameters(8);  // 2 int32_t parameters
        parameters[0] = (a >> 24) & 0xFF;
        parameters[1] = (a >> 16) & 0xFF;
        parameters[2] = (a >> 8) & 0xFF;
        parameters[3] = a & 0xFF;
        parameters[4] = (b >> 24) & 0xFF;
        parameters[5] = (b >> 16) & 0xFF;
        parameters[6] = (b >> 8) & 0xFF;
        parameters[7] = b & 0xFF;

        // Make synchronous RPC call
        RpcSyncResult result = client_.call_method_sync(
            CALCULATOR_SERVICE_ID, MULTIPLY_METHOD_ID, parameters);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "RPC call failed: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        // Deserialize result
        if (result.return_values.size() < 4) {
            std::cout << "Invalid response size" << std::endl;
            return;
        }

        int32_t product = (result.return_values[0] << 24) |
                         (result.return_values[1] << 16) |
                         (result.return_values[2] << 8) |
                         result.return_values[3];

        std::cout << "Result: " << a << " * " << b << " = " << product << std::endl;
        std::cout << "✓ MULTIPLY operation successful" << std::endl;
    }

    void test_get_stats() {
        std::cout << "\n--- Testing GET_STATS() ---" << std::endl;

        // No parameters needed
        std::vector<uint8_t> parameters;

        // Make synchronous RPC call
        RpcSyncResult result = client_.call_method_sync(
            CALCULATOR_SERVICE_ID, GET_STATS_METHOD_ID, parameters);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "RPC call failed: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        // Deserialize result
        if (result.return_values.size() < 4) {
            std::cout << "Invalid response size" << std::endl;
            return;
        }

        uint32_t call_count = (result.return_values[0] << 24) |
                             (result.return_values[1] << 16) |
                             (result.return_values[2] << 8) |
                             result.return_values[3];

        std::cout << "Server statistics: " << call_count << " total method calls processed" << std::endl;
        std::cout << "✓ GET_STATS operation successful" << std::endl;
    }
};

int main() {
    std::cout << "=== SOME/IP Method Calls Client ===" << std::endl;
    std::cout << std::endl;

    CalculatorClient client;

    if (!client.initialize()) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }

    // Give server time to start
    std::cout << "Waiting for server to be ready..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    client.run_calculations();

    client.shutdown();

    std::cout << "\nClient finished." << std::endl;
    return 0;
}
