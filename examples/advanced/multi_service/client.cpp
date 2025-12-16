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
 * @brief Multi-Service Client Example
 *
 * This example demonstrates a client interacting with multiple SOME/IP services:
 * - Calculator Service: Mathematical operations
 * - File System Service: File operations
 * - Sensor Service: Sensor data retrieval and configuration
 * - System Service: System information
 *
 * This shows enterprise-level service integration patterns.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>

#include <rpc/rpc_client.h>
#include <rpc/rpc_types.h>

using namespace someip;
using namespace someip::rpc;

// Service IDs (same as server)
const uint16_t CALCULATOR_SERVICE_ID = 0x6001;
const uint16_t FILESYSTEM_SERVICE_ID = 0x6002;
const uint16_t SENSOR_SERVICE_ID = 0x6003;
const uint16_t SYSTEM_SERVICE_ID = 0x6004;

// Calculator Service Methods
const uint16_t CALC_ADD_METHOD_ID = 0x0001;
const uint16_t CALC_SUBTRACT_METHOD_ID = 0x0002;
const uint16_t CALC_MULTIPLY_METHOD_ID = 0x0003;
const uint16_t CALC_DIVIDE_METHOD_ID = 0x0004;
const uint16_t CALC_GET_HISTORY_METHOD_ID = 0x0005;

// Filesystem Service Methods
const uint16_t FS_LIST_DIR_METHOD_ID = 0x0001;
const uint16_t FS_READ_FILE_METHOD_ID = 0x0002;
const uint16_t FS_WRITE_FILE_METHOD_ID = 0x0003;
const uint16_t FS_DELETE_FILE_METHOD_ID = 0x0004;
const uint16_t FS_GET_FILE_INFO_METHOD_ID = 0x0005;

// Sensor Service Methods
const uint16_t SENSOR_GET_READINGS_METHOD_ID = 0x0001;
const uint16_t SENSOR_SET_CONFIG_METHOD_ID = 0x0002;
const uint16_t SENSOR_CALIBRATE_METHOD_ID = 0x0003;

// System Service Methods
const uint16_t SYS_GET_INFO_METHOD_ID = 0x0001;
const uint16_t SYS_GET_LOAD_METHOD_ID = 0x0002;
const uint16_t SYS_SHUTDOWN_METHOD_ID = 0x0003;
const uint16_t SYS_RESTART_METHOD_ID = 0x0004;

// Sensor Event IDs
const uint16_t TEMPERATURE_EVENT_ID = 0x8001;
const uint16_t HUMIDITY_EVENT_ID = 0x8002;
const uint16_t PRESSURE_EVENT_ID = 0x8003;

class MultiServiceClient {
public:
    MultiServiceClient() : client_(0xABCD) {}

    bool initialize() {
        if (!client_.initialize()) {
            std::cerr << "Failed to initialize RPC client" << std::endl;
            return false;
        }

        std::cout << "Multi-Service Client initialized (ID: 0xABCD)" << std::endl;
        std::cout << "Connected to all available services" << std::endl;

        return true;
    }

    void run_demonstrations() {
        std::cout << "\n=== Multi-Service Demonstrations ===" << std::endl;

        // Demonstrate Calculator Service
        demonstrate_calculator_service();

        // Demonstrate Filesystem Service
        demonstrate_filesystem_service();

        // Demonstrate Sensor Service
        demonstrate_sensor_service();

        // Demonstrate System Service
        demonstrate_system_service();

        std::cout << "\n=== All Demonstrations Completed ===" << std::endl;
    }

    void shutdown() {
        client_.shutdown();
        std::cout << "Multi-Service Client shut down." << std::endl;
    }

private:
    RpcClient client_;

    // Calculator Service Demonstrations
    void demonstrate_calculator_service() {
        std::cout << "\n--- Calculator Service Demonstration ---" << std::endl;

        // Perform various calculations
        perform_calculation(CALCULATOR_SERVICE_ID, CALC_ADD_METHOD_ID, 15, 27, "15 + 27");
        perform_calculation(CALCULATOR_SERVICE_ID, CALC_SUBTRACT_METHOD_ID, 50, 23, "50 - 23");
        perform_calculation(CALCULATOR_SERVICE_ID, CALC_MULTIPLY_METHOD_ID, 8, 9, "8 * 9");
        perform_calculation(CALCULATOR_SERVICE_ID, CALC_DIVIDE_METHOD_ID, 144, 12, "144 / 12");

        // Get calculation history
        RpcSyncResult history_result = client_.call_method_sync(
            CALCULATOR_SERVICE_ID, CALC_GET_HISTORY_METHOD_ID, {});

        if (history_result.result == RpcResult::SUCCESS) {
            std::string history(history_result.return_values.begin(), history_result.return_values.end());
            std::cout << "\nCalculation History:\n" << history << std::endl;
        }
    }

    void perform_calculation(uint16_t service_id, uint16_t method_id,
                           int32_t a, int32_t b, const std::string& description) {
        // Serialize parameters
        std::vector<uint8_t> params(8);
        params[0] = (a >> 24) & 0xFF;
        params[1] = (a >> 16) & 0xFF;
        params[2] = (a >> 8) & 0xFF;
        params[3] = a & 0xFF;
        params[4] = (b >> 24) & 0xFF;
        params[5] = (b >> 16) & 0xFF;
        params[6] = (b >> 8) & 0xFF;
        params[7] = b & 0xFF;

        RpcSyncResult result = client_.call_method_sync(service_id, method_id, params);

        if (result.result == RpcResult::SUCCESS && result.return_values.size() >= 4) {
            int32_t calc_result = (result.return_values[0] << 24) |
                                (result.return_values[1] << 16) |
                                (result.return_values[2] << 8) |
                                result.return_values[3];

            std::cout << "✓ " << description << " = " << calc_result << std::endl;
        } else {
            std::cout << "❌ " << description << " failed" << std::endl;
        }
    }

    // Filesystem Service Demonstrations
    void demonstrate_filesystem_service() {
        std::cout << "\n--- Filesystem Service Demonstration ---" << std::endl;

        // List directory
        RpcSyncResult list_result = client_.call_method_sync(
            FILESYSTEM_SERVICE_ID, FS_LIST_DIR_METHOD_ID, {});

        if (list_result.result == RpcResult::SUCCESS) {
            std::string file_list(list_result.return_values.begin(), list_result.return_values.end());
            std::cout << "Files in system:\n" << file_list << std::endl;
        }

        // Read a file
        std::string filename = "/config/system.conf";
        std::vector<uint8_t> read_params(filename.begin(), filename.end());

        RpcSyncResult read_result = client_.call_method_sync(
            FILESYSTEM_SERVICE_ID, FS_READ_FILE_METHOD_ID, read_params);

        if (read_result.result == RpcResult::SUCCESS) {
            std::string file_content(read_result.return_values.begin(), read_result.return_values.end());
            std::cout << "\nContent of " << filename << ":\n" << file_content << std::endl;
        }

        // Write a new file
        std::string new_filename = "/data/user_data.txt";
        std::string new_content = "Hello from multi-service client!\nTimestamp: " +
                                std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        std::vector<uint8_t> write_params;
        write_params.insert(write_params.end(), new_filename.begin(), new_filename.end());
        write_params.push_back('\0');  // Null terminator
        write_params.insert(write_params.end(), new_content.begin(), new_content.end());

        RpcSyncResult write_result = client_.call_method_sync(
            FILESYSTEM_SERVICE_ID, FS_WRITE_FILE_METHOD_ID, write_params);

        if (write_result.result == RpcResult::SUCCESS) {
            std::cout << "✓ Created file: " << new_filename << std::endl;
        }

        // Get file info
        std::vector<uint8_t> info_params(new_filename.begin(), new_filename.end());

        RpcSyncResult info_result = client_.call_method_sync(
            FILESYSTEM_SERVICE_ID, FS_GET_FILE_INFO_METHOD_ID, info_params);

        if (info_result.result == RpcResult::SUCCESS) {
            std::string file_info(info_result.return_values.begin(), info_result.return_values.end());
            std::cout << "File info:\n" << file_info << std::endl;
        }
    }

    // Sensor Service Demonstrations
    void demonstrate_sensor_service() {
        std::cout << "\n--- Sensor Service Demonstration ---" << std::endl;

        // Get current sensor readings
        RpcSyncResult readings_result = client_.call_method_sync(
            SENSOR_SERVICE_ID, SENSOR_GET_READINGS_METHOD_ID, {});

        if (readings_result.result == RpcResult::SUCCESS && readings_result.return_values.size() >= 12) {
            // Parse sensor readings (3 floats)
            auto parse_float = [](const std::vector<uint8_t>& data, size_t offset) -> float {
                uint32_t bits = (data[offset] << 24) | (data[offset+1] << 16) |
                              (data[offset+2] << 8) | data[offset+3];
                float value;
                std::memcpy(&value, &bits, sizeof(float));
                return value;
            };

            float temperature = parse_float(readings_result.return_values, 0);
            float humidity = parse_float(readings_result.return_values, 4);
            float pressure = parse_float(readings_result.return_values, 8);

            std::cout << "Current Sensor Readings:" << std::endl;
            std::cout << "  Temperature: " << std::fixed << std::setprecision(1) << temperature << "°C" << std::endl;
            std::cout << "  Humidity: " << humidity << "%" << std::endl;
            std::cout << "  Pressure: " << pressure << " hPa" << std::endl;
        }

        // Configure sensors (enable all, 3-second interval)
        std::vector<uint8_t> config_params = {1, 1, 1, 0, 0, 0, 3};  // temp, humidity, pressure enabled, 3s interval
        config_params[3] = (3000 >> 24) & 0xFF;
        config_params[4] = (3000 >> 16) & 0xFF;
        config_params[5] = (3000 >> 8) & 0xFF;
        config_params[6] = 3000 & 0xFF;

        RpcSyncResult config_result = client_.call_method_sync(
            SENSOR_SERVICE_ID, SENSOR_SET_CONFIG_METHOD_ID, config_params);

        if (config_result.result == RpcResult::SUCCESS) {
            std::cout << "✓ Sensor configuration updated (3-second intervals)" << std::endl;
        }

        // Calibrate sensors
        RpcSyncResult calib_result = client_.call_method_sync(
            SENSOR_SERVICE_ID, SENSOR_CALIBRATE_METHOD_ID, {});

        if (calib_result.result == RpcResult::SUCCESS) {
            std::cout << "✓ Sensor calibration completed" << std::endl;
        }
    }

    // System Service Demonstrations
    void demonstrate_system_service() {
        std::cout << "\n--- System Service Demonstration ---" << std::endl;

        // Get system information
        RpcSyncResult info_result = client_.call_method_sync(
            SYSTEM_SERVICE_ID, SYS_GET_INFO_METHOD_ID, {});

        if (info_result.result == RpcResult::SUCCESS) {
            std::string system_info(info_result.return_values.begin(), info_result.return_values.end());
            std::cout << "System Information:\n" << system_info << std::endl;
        }

        // Get system load
        RpcSyncResult load_result = client_.call_method_sync(
            SYSTEM_SERVICE_ID, SYS_GET_LOAD_METHOD_ID, {});

        if (load_result.result == RpcResult::SUCCESS && load_result.return_values.size() >= 12) {
            auto parse_float = [](const std::vector<uint8_t>& data, size_t offset) -> float {
                uint32_t bits = (data[offset] << 24) | (data[offset+1] << 16) |
                              (data[offset+2] << 8) | data[offset+3];
                float value;
                std::memcpy(&value, &bits, sizeof(float));
                return value;
            };

            float cpu_load = parse_float(load_result.return_values, 0);
            float memory_load = parse_float(load_result.return_values, 4);
            uint32_t connections = (load_result.return_values[8] << 24) |
                                 (load_result.return_values[9] << 16) |
                                 (load_result.return_values[10] << 8) |
                                 load_result.return_values[11];

            std::cout << "System Load:" << std::endl;
            std::cout << "  CPU Usage: " << cpu_load << "%" << std::endl;
            std::cout << "  Memory Usage: " << memory_load << "%" << std::endl;
            std::cout << "  Active Connections: " << connections << std::endl;
        }
    }

};

int main() {
    std::cout << "=== SOME/IP Multi-Service Client ===" << std::endl;
    std::cout << std::endl;

    MultiServiceClient client;

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
