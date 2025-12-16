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
 * @brief Multi-Service Server Example
 *
 * This example demonstrates a server offering multiple SOME/IP services
 * simultaneously:
 * - Calculator Service: Mathematical operations
 * - File System Service: File operations simulation
 * - Sensor Service: Sensor data management
 * - System Service: System information and control
 *
 * This shows enterprise-level service architecture patterns.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <cstring>
#include <algorithm>

#include <rpc/rpc_server.h>
#include <rpc/rpc_types.h>
#include <events/event_publisher.h>
#include <events/event_types.h>

using namespace someip;
using namespace someip::rpc;
using namespace someip::events;

// Service IDs
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

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

class MultiServiceServer {
public:
    MultiServiceServer()
        : calculator_server_(CALCULATOR_SERVICE_ID),
          filesystem_server_(FILESYSTEM_SERVICE_ID),
          sensor_server_(SENSOR_SERVICE_ID),
          system_server_(SYSTEM_SERVICE_ID),
          sensor_publisher_(SENSOR_SERVICE_ID, 0x0001) {}

    bool initialize() {
        // Initialize all services
        if (!initialize_calculator_service()) {
            std::cerr << "Failed to initialize calculator service" << std::endl;
            return false;
        }

        if (!initialize_filesystem_service()) {
            std::cerr << "Failed to initialize filesystem service" << std::endl;
            return false;
        }

        if (!initialize_sensor_service()) {
            std::cerr << "Failed to initialize sensor service" << std::endl;
            return false;
        }

        if (!initialize_system_service()) {
            std::cerr << "Failed to initialize system service" << std::endl;
            return false;
        }

        std::cout << "Multi-Service Server initialized successfully!" << std::endl;
        std::cout << "Available services:" << std::endl;
        std::cout << "  - Calculator Service (0x" << std::hex << CALCULATOR_SERVICE_ID << ")" << std::endl;
        std::cout << "  - Filesystem Service (0x" << std::hex << FILESYSTEM_SERVICE_ID << ")" << std::endl;
        std::cout << "  - Sensor Service (0x" << std::hex << SENSOR_SERVICE_ID << ")" << std::endl;
        std::cout << "  - System Service (0x" << std::hex << SYSTEM_SERVICE_ID << ")" << std::endl;

        return true;
    }

    void run() {
        std::cout << "\nMulti-Service Server running. Press Ctrl+C to exit." << std::endl;
        std::cout << "All services are active and accepting requests..." << std::endl;

        // Start sensor data publishing thread
        std::thread sensor_thread([this]() {
            run_sensor_publisher();
        });
        sensor_thread.detach();

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        shutdown();
        std::cout << "Multi-Service Server shut down." << std::endl;
    }

private:
    RpcServer calculator_server_;
    RpcServer filesystem_server_;
    RpcServer sensor_server_;
    RpcServer system_server_;
    EventPublisher sensor_publisher_;

    // Calculation history for demonstration
    std::vector<std::string> calc_history_;
    std::mutex calc_mutex_;

    // Simulated filesystem (in-memory for demo)
    std::unordered_map<std::string, std::string> filesystem_;
    std::mutex fs_mutex_;

    // Sensor configuration
    struct SensorConfig {
        bool temperature_enabled = true;
        bool humidity_enabled = true;
        bool pressure_enabled = true;
        uint32_t update_interval_ms = 5000;  // 5 seconds
    } sensor_config_;

    std::mutex sensor_mutex_;

    void shutdown() {
        sensor_publisher_.shutdown();
        system_server_.shutdown();
        sensor_server_.shutdown();
        filesystem_server_.shutdown();
        calculator_server_.shutdown();
    }

    // Calculator Service Implementation
    bool initialize_calculator_service() {
        calculator_server_.register_method(CALC_ADD_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_calculator_op(client_id, session_id, input, output, "ADD", [](int32_t a, int32_t b) { return a + b; });
            });

        calculator_server_.register_method(CALC_SUBTRACT_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_calculator_op(client_id, session_id, input, output, "SUBTRACT", [](int32_t a, int32_t b) { return a - b; });
            });

        calculator_server_.register_method(CALC_MULTIPLY_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_calculator_op(client_id, session_id, input, output, "MULTIPLY", [](int32_t a, int32_t b) { return a * b; });
            });

        calculator_server_.register_method(CALC_DIVIDE_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_calculator_op(client_id, session_id, input, output, "DIVIDE",
                    [](int32_t a, int32_t b) -> int32_t {
                        if (b == 0) throw std::runtime_error("Division by zero");
                        return a / b;
                    });
            });

        calculator_server_.register_method(CALC_GET_HISTORY_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_get_calc_history(client_id, session_id, input, output);
            });

        return calculator_server_.initialize();
    }

    RpcResult handle_calculator_op(uint16_t client_id, uint16_t session_id,
                                  const std::vector<uint8_t>& input, std::vector<uint8_t>& output,
                                  const std::string& op_name,
                                  std::function<int32_t(int32_t, int32_t)> operation) {
        if (input.size() < 8) {
            return RpcResult::INVALID_PARAMETERS;
        }

        int32_t a = (input[0] << 24) | (input[1] << 16) | (input[2] << 8) | input[3];
        int32_t b = (input[4] << 24) | (input[5] << 16) | (input[6] << 8) | input[7];

        try {
            int32_t result = operation(a, b);

            std::string history_entry = op_name + ": " + std::to_string(a) + " " +
                                      (op_name == "DIVIDE" ? "/" : op_name == "MULTIPLY" ? "*" :
                                       op_name == "ADD" ? "+" : "-") + " " +
                                      std::to_string(b) + " = " + std::to_string(result);

            {
                std::lock_guard<std::mutex> lock(calc_mutex_);
                calc_history_.push_back(history_entry);
                if (calc_history_.size() > 10) {  // Keep last 10 operations
                    calc_history_.erase(calc_history_.begin());
                }
            }

            std::cout << "[CALCULATOR] " << history_entry << std::endl;

            // Return result
            output.resize(4);
            output[0] = (result >> 24) & 0xFF;
            output[1] = (result >> 16) & 0xFF;
            output[2] = (result >> 8) & 0xFF;
            output[3] = result & 0xFF;

            return RpcResult::SUCCESS;
        } catch (const std::exception& e) {
            std::cout << "[CALCULATOR] Error: " << e.what() << std::endl;
            return RpcResult::INVALID_PARAMETERS;
        }
    }

    RpcResult handle_get_calc_history(uint16_t client_id, uint16_t session_id,
                                    const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::lock_guard<std::mutex> lock(calc_mutex_);

        std::string history;
        for (size_t i = 0; i < calc_history_.size(); ++i) {
            if (i > 0) history += "\n";
            history += calc_history_[i];
        }

        std::cout << "[CALCULATOR] Returning calculation history (" << calc_history_.size() << " entries)" << std::endl;

        // Serialize as string
        output.assign(history.begin(), history.end());
        return RpcResult::SUCCESS;
    }

    // Filesystem Service Implementation
    bool initialize_filesystem_service() {
        filesystem_server_.register_method(FS_LIST_DIR_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_list_dir(client_id, session_id, input, output);
            });

        filesystem_server_.register_method(FS_READ_FILE_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_read_file(client_id, session_id, input, output);
            });

        filesystem_server_.register_method(FS_WRITE_FILE_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_write_file(client_id, session_id, input, output);
            });

        filesystem_server_.register_method(FS_DELETE_FILE_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_delete_file(client_id, session_id, input, output);
            });

        filesystem_server_.register_method(FS_GET_FILE_INFO_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_get_file_info(client_id, session_id, input, output);
            });

        // Initialize with some sample files
        {
            std::lock_guard<std::mutex> lock(fs_mutex_);
            filesystem_["/config/system.conf"] = "# System configuration\nversion=1.0\n";
            filesystem_["/logs/app.log"] = "2024-01-01 10:00:00 INFO Application started\n";
            filesystem_["/data/sensor.csv"] = "timestamp,temperature,humidity\n1640995200,23.5,65.2\n";
        }

        return filesystem_server_.initialize();
    }

    RpcResult handle_list_dir(uint16_t client_id, uint16_t session_id,
                            const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::lock_guard<std::mutex> lock(fs_mutex_);

        std::string file_list;
        for (const auto& file : filesystem_) {
            if (!file_list.empty()) file_list += "\n";
            file_list += file.first;
        }

        std::cout << "[FILESYSTEM] Listed " << filesystem_.size() << " files" << std::endl;

        output.assign(file_list.begin(), file_list.end());
        return RpcResult::SUCCESS;
    }

    RpcResult handle_read_file(uint16_t client_id, uint16_t session_id,
                             const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        if (input.empty()) {
            return RpcResult::INVALID_PARAMETERS;
        }

        std::string filename(input.begin(), input.end());

        std::lock_guard<std::mutex> lock(fs_mutex_);
        auto it = filesystem_.find(filename);

        if (it == filesystem_.end()) {
            std::cout << "[FILESYSTEM] File not found: " << filename << std::endl;
            return RpcResult::INVALID_PARAMETERS;  // File not found
        }

        std::cout << "[FILESYSTEM] Read file: " << filename << " (" << it->second.size() << " bytes)" << std::endl;

        output.assign(it->second.begin(), it->second.end());
        return RpcResult::SUCCESS;
    }

    RpcResult handle_write_file(uint16_t client_id, uint16_t session_id,
                              const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        if (input.size() < 2) {  // Need at least filename null terminator + 1 data byte
            return RpcResult::INVALID_PARAMETERS;
        }

        // Find null terminator separating filename from data
        auto separator = std::find(input.begin(), input.end(), '\0');
        if (separator == input.end()) {
            return RpcResult::INVALID_PARAMETERS;
        }

        std::string filename(input.begin(), separator);
        std::string data(separator + 1, input.end());

        {
            std::lock_guard<std::mutex> lock(fs_mutex_);
            filesystem_[filename] = data;
        }

        std::cout << "[FILESYSTEM] Wrote file: " << filename << " (" << data.size() << " bytes)" << std::endl;

        output.resize(4);
        output[0] = 0; // Success
        return RpcResult::SUCCESS;
    }

    RpcResult handle_delete_file(uint16_t client_id, uint16_t session_id,
                               const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        if (input.empty()) {
            return RpcResult::INVALID_PARAMETERS;
        }

        std::string filename(input.begin(), input.end());

        std::lock_guard<std::mutex> lock(fs_mutex_);
        auto erased = filesystem_.erase(filename);

        if (erased > 0) {
            std::cout << "[FILESYSTEM] Deleted file: " << filename << std::endl;
            output.resize(4);
            output[0] = 0; // Success
        } else {
            std::cout << "[FILESYSTEM] File not found for deletion: " << filename << std::endl;
            output.resize(4);
            output[0] = 1; // Not found
        }

        return RpcResult::SUCCESS;
    }

    RpcResult handle_get_file_info(uint16_t client_id, uint16_t session_id,
                                 const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        if (input.empty()) {
            return RpcResult::INVALID_PARAMETERS;
        }

        std::string filename(input.begin(), input.end());

        std::lock_guard<std::mutex> lock(fs_mutex_);
        auto it = filesystem_.find(filename);

        if (it == filesystem_.end()) {
            std::cout << "[FILESYSTEM] File not found: " << filename << std::endl;
            return RpcResult::INVALID_PARAMETERS;
        }

        std::string info = "File: " + filename + "\nSize: " + std::to_string(it->second.size()) + " bytes";
        output.assign(info.begin(), info.end());

        std::cout << "[FILESYSTEM] File info: " << filename << " (" << it->second.size() << " bytes)" << std::endl;

        return RpcResult::SUCCESS;
    }

    // Sensor Service Implementation
    bool initialize_sensor_service() {
        sensor_server_.register_method(SENSOR_GET_READINGS_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_get_sensor_readings(client_id, session_id, input, output);
            });

        sensor_server_.register_method(SENSOR_SET_CONFIG_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_set_sensor_config(client_id, session_id, input, output);
            });

        sensor_server_.register_method(SENSOR_CALIBRATE_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_calibrate_sensor(client_id, session_id, input, output);
            });

        // Initialize sensor publisher
        EventConfig temp_config;
        temp_config.event_id = TEMPERATURE_EVENT_ID;
        temp_config.eventgroup_id = 0x0001;
        temp_config.reliability = Reliability::UNRELIABLE;
        temp_config.notification_type = NotificationType::PERIODIC;
        temp_config.cycle_time = std::chrono::milliseconds(sensor_config_.update_interval_ms);

        EventConfig humidity_config;
        humidity_config.event_id = HUMIDITY_EVENT_ID;
        humidity_config.eventgroup_id = 0x0001;
        humidity_config.reliability = Reliability::UNRELIABLE;
        humidity_config.notification_type = NotificationType::PERIODIC;
        humidity_config.cycle_time = std::chrono::milliseconds(sensor_config_.update_interval_ms);

        EventConfig pressure_config;
        pressure_config.event_id = PRESSURE_EVENT_ID;
        pressure_config.eventgroup_id = 0x0001;
        pressure_config.reliability = Reliability::UNRELIABLE;
        pressure_config.notification_type = NotificationType::PERIODIC;
        pressure_config.cycle_time = std::chrono::milliseconds(sensor_config_.update_interval_ms);

        sensor_publisher_.register_event(temp_config);
        sensor_publisher_.register_event(humidity_config);
        sensor_publisher_.register_event(pressure_config);
        sensor_publisher_.initialize();

        return sensor_server_.initialize();
    }

    RpcResult handle_get_sensor_readings(uint16_t client_id, uint16_t session_id,
                                       const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::lock_guard<std::mutex> lock(sensor_mutex_);

        // Generate current sensor readings
        float temperature = 22.5f + (rand() % 100 - 50) / 10.0f;  // 17.5-27.5°C
        float humidity = 60.0f + (rand() % 200 - 100) / 10.0f;    // 50-70%
        float pressure = 1013.25f + (rand() % 100 - 50) / 10.0f;  // 1008-1018 hPa

        std::cout << "[SENSOR] Readings - Temp: " << temperature << "°C, Humidity: "
                  << humidity << "%, Pressure: " << pressure << " hPa" << std::endl;

        // Serialize readings (big-endian floats)
        output.resize(12);  // 3 floats * 4 bytes each

        // Temperature
        uint32_t temp_bits;
        std::memcpy(&temp_bits, &temperature, sizeof(float));
        output[0] = (temp_bits >> 24) & 0xFF;
        output[1] = (temp_bits >> 16) & 0xFF;
        output[2] = (temp_bits >> 8) & 0xFF;
        output[3] = temp_bits & 0xFF;

        // Humidity
        uint32_t humidity_bits;
        std::memcpy(&humidity_bits, &humidity, sizeof(float));
        output[4] = (humidity_bits >> 24) & 0xFF;
        output[5] = (humidity_bits >> 16) & 0xFF;
        output[6] = (humidity_bits >> 8) & 0xFF;
        output[7] = humidity_bits & 0xFF;

        // Pressure
        uint32_t pressure_bits;
        std::memcpy(&pressure_bits, &pressure, sizeof(float));
        output[8] = (pressure_bits >> 24) & 0xFF;
        output[9] = (pressure_bits >> 16) & 0xFF;
        output[10] = (pressure_bits >> 8) & 0xFF;
        output[11] = pressure_bits & 0xFF;

        return RpcResult::SUCCESS;
    }

    RpcResult handle_set_sensor_config(uint16_t client_id, uint16_t session_id,
                                     const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        if (input.size() < 7) {  // 3 booleans + 4-byte interval
            return RpcResult::INVALID_PARAMETERS;
        }

        std::lock_guard<std::mutex> lock(sensor_mutex_);

        sensor_config_.temperature_enabled = input[0] != 0;
        sensor_config_.humidity_enabled = input[1] != 0;
        sensor_config_.pressure_enabled = input[2] != 0;

        sensor_config_.update_interval_ms = (input[3] << 24) | (input[4] << 16) | (input[5] << 8) | input[6];

        std::cout << "[SENSOR] Configuration updated - Temp: " << (sensor_config_.temperature_enabled ? "ON" : "OFF")
                  << ", Humidity: " << (sensor_config_.humidity_enabled ? "ON" : "OFF")
                  << ", Pressure: " << (sensor_config_.pressure_enabled ? "ON" : "OFF")
                  << ", Interval: " << sensor_config_.update_interval_ms << "ms" << std::endl;

        output.resize(4);
        output[0] = 0; // Success
        return RpcResult::SUCCESS;
    }

    RpcResult handle_calibrate_sensor(uint16_t client_id, uint16_t session_id,
                                    const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::cout << "[SENSOR] Starting sensor calibration..." << std::endl;

        // Simulate calibration process
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "[SENSOR] Sensor calibration completed" << std::endl;

        output.resize(4);
        output[0] = 0; // Success
        return RpcResult::SUCCESS;
    }

    void run_sensor_publisher() {
        while (running) {
            std::lock_guard<std::mutex> lock(sensor_mutex_);

            if (sensor_config_.temperature_enabled) {
                float temperature = 22.5f + (rand() % 100 - 50) / 10.0f;

                uint32_t temp_bits;
                std::memcpy(&temp_bits, &temperature, sizeof(float));
                std::vector<uint8_t> temp_data(4);
                temp_data[0] = (temp_bits >> 24) & 0xFF;
                temp_data[1] = (temp_bits >> 16) & 0xFF;
                temp_data[2] = (temp_bits >> 8) & 0xFF;
                temp_data[3] = temp_bits & 0xFF;

                sensor_publisher_.publish_event(TEMPERATURE_EVENT_ID, temp_data);
            }

            if (sensor_config_.humidity_enabled) {
                float humidity = 60.0f + (rand() % 200 - 100) / 10.0f;

                uint32_t humidity_bits;
                std::memcpy(&humidity_bits, &humidity, sizeof(float));
                std::vector<uint8_t> humidity_data(4);
                humidity_data[0] = (humidity_bits >> 24) & 0xFF;
                humidity_data[1] = (humidity_bits >> 16) & 0xFF;
                humidity_data[2] = (humidity_bits >> 8) & 0xFF;
                humidity_data[3] = humidity_bits & 0xFF;

                sensor_publisher_.publish_event(HUMIDITY_EVENT_ID, humidity_data);
            }

            if (sensor_config_.pressure_enabled) {
                float pressure = 1013.25f + (rand() % 100 - 50) / 10.0f;

                uint32_t pressure_bits;
                std::memcpy(&pressure_bits, &pressure, sizeof(float));
                std::vector<uint8_t> pressure_data(4);
                pressure_data[0] = (pressure_bits >> 24) & 0xFF;
                pressure_data[1] = (pressure_bits >> 16) & 0xFF;
                pressure_data[2] = (pressure_bits >> 8) & 0xFF;
                pressure_data[3] = pressure_bits & 0xFF;

                sensor_publisher_.publish_event(PRESSURE_EVENT_ID, pressure_data);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(sensor_config_.update_interval_ms));
        }
    }

    // System Service Implementation
    bool initialize_system_service() {
        system_server_.register_method(SYS_GET_INFO_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_get_system_info(client_id, session_id, input, output);
            });

        system_server_.register_method(SYS_GET_LOAD_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_get_system_load(client_id, session_id, input, output);
            });

        system_server_.register_method(SYS_SHUTDOWN_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_system_shutdown(client_id, session_id, input, output);
            });

        system_server_.register_method(SYS_RESTART_METHOD_ID,
            [this](uint16_t client_id, uint16_t session_id,
                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) -> RpcResult {
                return handle_system_restart(client_id, session_id, input, output);
            });

        return system_server_.initialize();
    }

    RpcResult handle_get_system_info(uint16_t client_id, uint16_t session_id,
                                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::string info = "Multi-Service SOME/IP Server\n";
        info += "Version: 1.0.0\n";
        info += "Services: Calculator, Filesystem, Sensor, System\n";
        info += "Uptime: Running\n";
        info += "Status: All systems operational";

        std::cout << "[SYSTEM] System information requested" << std::endl;

        output.assign(info.begin(), info.end());
        return RpcResult::SUCCESS;
    }

    RpcResult handle_get_system_load(uint16_t client_id, uint16_t session_id,
                                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        // Simulate system load
        float cpu_load = 25.0f + (rand() % 500) / 10.0f;    // 25-75%
        float memory_load = 40.0f + (rand() % 300) / 10.0f; // 40-70%
        uint32_t active_connections = 4;  // Our 4 services

        std::cout << "[SYSTEM] Load - CPU: " << cpu_load << "%, Memory: " << memory_load
                  << "%, Connections: " << active_connections << std::endl;

        // Serialize load data
        output.resize(12);  // 2 floats + 1 uint32

        // CPU load
        uint32_t cpu_bits;
        std::memcpy(&cpu_bits, &cpu_load, sizeof(float));
        output[0] = (cpu_bits >> 24) & 0xFF;
        output[1] = (cpu_bits >> 16) & 0xFF;
        output[2] = (cpu_bits >> 8) & 0xFF;
        output[3] = cpu_bits & 0xFF;

        // Memory load
        uint32_t mem_bits;
        std::memcpy(&mem_bits, &memory_load, sizeof(float));
        output[4] = (mem_bits >> 24) & 0xFF;
        output[5] = (mem_bits >> 16) & 0xFF;
        output[6] = (mem_bits >> 8) & 0xFF;
        output[7] = mem_bits & 0xFF;

        // Active connections
        output[8] = (active_connections >> 24) & 0xFF;
        output[9] = (active_connections >> 16) & 0xFF;
        output[10] = (active_connections >> 8) & 0xFF;
        output[11] = active_connections & 0xFF;

        return RpcResult::SUCCESS;
    }

    RpcResult handle_system_shutdown(uint16_t client_id, uint16_t session_id,
                                   const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::cout << "[SYSTEM] Shutdown requested by client 0x" << std::hex << client_id << std::endl;
        std::cout << "Note: This is a demo - actual shutdown not performed" << std::endl;

        output.resize(4);
        output[0] = 0; // Success (would normally require authentication)
        return RpcResult::SUCCESS;
    }

    RpcResult handle_system_restart(uint16_t client_id, uint16_t session_id,
                                  const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        std::cout << "[SYSTEM] Restart requested by client 0x" << std::hex << client_id << std::endl;
        std::cout << "Note: This is a demo - actual restart not performed" << std::endl;

        output.resize(4);
        output[0] = 0; // Success (would normally require authentication)
        return RpcResult::SUCCESS;
    }
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== SOME/IP Multi-Service Server ===" << std::endl;
    std::cout << std::endl;

    MultiServiceServer server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    server.run();

    return 0;
}
