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
 * @brief Complex Types Server Example
 *
 * This example demonstrates serialization/deserialization of complex data types:
 * - Structures with multiple fields
 * - Arrays of primitive and complex types
 * - Nested structures
 * - Variable-length data
 *
 * This shows advanced SOME/IP serialization patterns.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>
#include <array>
#include <string>

#include <rpc/rpc_server.h>
#include <rpc/rpc_types.h>
#include <serialization/serializer.h>

using namespace someip;
using namespace someip::rpc;
using namespace someip::serialization;

// Service and method IDs
const uint16_t COMPLEX_SERVICE_ID = 0x4000;
const uint16_t PROCESS_VEHICLE_DATA_METHOD_ID = 0x0001;
const uint16_t GET_SENSOR_ARRAY_METHOD_ID = 0x0002;
const uint16_t ECHO_COMPLEX_STRUCT_METHOD_ID = 0x0003;

// Data structures for complex types
struct VehicleData {
    uint32_t vehicle_id;
    std::string model;
    float fuel_level;      // 0.0 - 1.0
    uint8_t tire_pressure[4]; // 4 tires
    bool lights_on;
    uint16_t mileage;      // km
};

struct SensorReading {
    uint8_t sensor_id;
    float value;
    std::string unit;
    uint32_t timestamp;
};

struct SensorArray {
    uint32_t array_size;
    std::vector<SensorReading> sensors;
};

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

class ComplexTypesServer {
public:
    ComplexTypesServer() : server_(COMPLEX_SERVICE_ID) {}

    bool initialize() {
        // Register method handlers
        server_.register_method(PROCESS_VEHICLE_DATA_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                                       const std::vector<uint8_t>& input,
                                                                       std::vector<uint8_t>& output) -> RpcResult {
            return handle_process_vehicle_data(client_id, session_id, input, output);
        });

        server_.register_method(GET_SENSOR_ARRAY_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                                  const std::vector<uint8_t>& input,
                                                                  std::vector<uint8_t>& output) -> RpcResult {
            return handle_get_sensor_array(client_id, session_id, input, output);
        });

        server_.register_method(ECHO_COMPLEX_STRUCT_METHOD_ID, [this](uint16_t client_id, uint16_t session_id,
                                                                     const std::vector<uint8_t>& input,
                                                                     std::vector<uint8_t>& output) -> RpcResult {
            return handle_echo_complex_struct(client_id, session_id, input, output);
        });

        if (!server_.initialize()) {
            std::cerr << "Failed to initialize RPC server" << std::endl;
            return false;
        }

        std::cout << "Complex Types Server initialized for service 0x" << std::hex << COMPLEX_SERVICE_ID << std::endl;
        std::cout << "Available methods:" << std::endl;
        std::cout << "  - 0x" << std::hex << PROCESS_VEHICLE_DATA_METHOD_ID << ": process_vehicle_data(VehicleData) -> string" << std::endl;
        std::cout << "  - 0x" << std::hex << GET_SENSOR_ARRAY_METHOD_ID << ": get_sensor_array() -> SensorArray" << std::endl;
        std::cout << "  - 0x" << std::hex << ECHO_COMPLEX_STRUCT_METHOD_ID << ": echo_complex_struct(SensorReading) -> SensorReading" << std::endl;

        return true;
    }

    void run() {
        std::cout << "Complex Types Server running. Press Ctrl+C to exit." << std::endl;

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        server_.shutdown();
        std::cout << "Complex Types Server shut down." << std::endl;
    }

private:
    RpcServer server_;

    // Serialization helpers for complex types
    std::vector<uint8_t> serialize_vehicle_data(const VehicleData& data) {
        Serializer serializer;

        // Serialize fields
        serializer.serialize_uint32(data.vehicle_id);
        serializer.serialize_string(data.model);
        serializer.serialize_float(data.fuel_level);

        // Serialize tire pressure array (fixed size 4)
        for (uint8_t pressure : data.tire_pressure) {
            serializer.serialize_uint8(pressure);
        }

        serializer.serialize_bool(data.lights_on);
        serializer.serialize_uint16(data.mileage);

        return serializer.get_buffer();
    }

    DeserializationResult<VehicleData> deserialize_vehicle_data(Deserializer& deserializer) {
        VehicleData data;

        // Deserialize fields
        auto vehicle_id = deserializer.deserialize_uint32();
        if (vehicle_id.is_error()) return DeserializationResult<VehicleData>::error(vehicle_id.get_error());
        data.vehicle_id = vehicle_id.get_value();

        auto model = deserializer.deserialize_string();
        if (model.is_error()) return DeserializationResult<VehicleData>::error(model.get_error());
        data.model = model.get_value();

        auto fuel_level = deserializer.deserialize_float();
        if (fuel_level.is_error()) return DeserializationResult<VehicleData>::error(fuel_level.get_error());
        data.fuel_level = fuel_level.get_value();

        // Deserialize tire pressure array
        for (int i = 0; i < 4; ++i) {
            auto pressure = deserializer.deserialize_uint8();
            if (pressure.is_error()) return DeserializationResult<VehicleData>::error(pressure.get_error());
            data.tire_pressure[i] = pressure.get_value();
        }

        auto lights_on = deserializer.deserialize_bool();
        if (lights_on.is_error()) return DeserializationResult<VehicleData>::error(lights_on.get_error());
        data.lights_on = lights_on.get_value();

        auto mileage = deserializer.deserialize_uint16();
        if (mileage.is_error()) return DeserializationResult<VehicleData>::error(mileage.get_error());
        data.mileage = mileage.get_value();

        return DeserializationResult<VehicleData>::success(data);
    }

    std::vector<uint8_t> serialize_sensor_reading(const SensorReading& data) {
        Serializer serializer;
        serializer.serialize_uint8(data.sensor_id);
        serializer.serialize_float(data.value);
        serializer.serialize_string(data.unit);
        serializer.serialize_uint32(data.timestamp);
        return serializer.get_buffer();
    }

    DeserializationResult<SensorReading> deserialize_sensor_reading(Deserializer& deserializer) {
        SensorReading data;

        auto sensor_id = deserializer.deserialize_uint8();
        if (sensor_id.is_error()) return DeserializationResult<SensorReading>::error(sensor_id.get_error());
        data.sensor_id = sensor_id.get_value();

        auto value = deserializer.deserialize_float();
        if (value.is_error()) return DeserializationResult<SensorReading>::error(value.get_error());
        data.value = value.get_value();

        auto unit = deserializer.deserialize_string();
        if (unit.is_error()) return DeserializationResult<SensorReading>::error(unit.get_error());
        data.unit = unit.get_value();

        auto timestamp = deserializer.deserialize_uint32();
        if (timestamp.is_error()) return DeserializationResult<SensorReading>::error(timestamp.get_error());
        data.timestamp = timestamp.get_value();

        return DeserializationResult<SensorReading>::success(data);
    }

    std::vector<uint8_t> serialize_sensor_array(const SensorArray& data) {
        Serializer serializer;

        // Serialize array size first
        serializer.serialize_uint32(data.sensors.size());

        // Serialize each sensor reading
        for (const auto& sensor : data.sensors) {
            // Calculate and serialize sensor data size
            size_t sensor_size = 1 + 4 + 4 + sensor.unit.size() + 4;  // id + value + unit_len + unit + timestamp
            serializer.serialize_uint32(sensor_size);

            // Serialize sensor data directly
            serializer.serialize_uint8(sensor.sensor_id);
            serializer.serialize_float(sensor.value);
            serializer.serialize_string(sensor.unit);
            serializer.serialize_uint32(sensor.timestamp);
        }

        return serializer.get_buffer();
    }

    RpcResult handle_process_vehicle_data(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                        const std::vector<uint8_t>& input,
                                        std::vector<uint8_t>& output) {
        Deserializer deserializer(input);
        auto vehicle_result = deserialize_vehicle_data(deserializer);

        if (vehicle_result.is_error()) {
            std::cout << "Failed to deserialize vehicle data" << std::endl;
            return RpcResult::INVALID_PARAMETERS;
        }

        const VehicleData& vehicle = vehicle_result.get_value();

        std::cout << "Processing vehicle data:" << std::endl;
        std::cout << "  ID: " << vehicle.vehicle_id << std::endl;
        std::cout << "  Model: " << vehicle.model << std::endl;
        std::cout << "  Fuel Level: " << (vehicle.fuel_level * 100) << "%" << std::endl;
        std::cout << "  Tire Pressure: "
                  << (int)vehicle.tire_pressure[0] << ", "
                  << (int)vehicle.tire_pressure[1] << ", "
                  << (int)vehicle.tire_pressure[2] << ", "
                  << (int)vehicle.tire_pressure[3] << " PSI" << std::endl;
        std::cout << "  Lights: " << (vehicle.lights_on ? "ON" : "OFF") << std::endl;
        std::cout << "  Mileage: " << vehicle.mileage << " km" << std::endl;

        // Create response message
        std::string response = "Processed vehicle data for " + vehicle.model +
                              " (ID: " + std::to_string(vehicle.vehicle_id) + ")";

        Serializer response_serializer;
        response_serializer.serialize_string(response);
        output = response_serializer.get_buffer();

        return RpcResult::SUCCESS;
    }

    RpcResult handle_get_sensor_array(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                    const std::vector<uint8_t>& /*input*/,
                                    std::vector<uint8_t>& output) {
        // Create sample sensor data
        SensorArray sensor_array;
        sensor_array.sensors = {
            {1, 23.5f, "°C", 1000000},
            {2, 65.2f, "%", 1000001},
            {3, 12.8f, "V", 1000002},
            {4, 1013.25f, "hPa", 1000003}
        };

        std::cout << "Returning sensor array with " << sensor_array.sensors.size() << " readings" << std::endl;

        output = serialize_sensor_array(sensor_array);
        return RpcResult::SUCCESS;
    }

    RpcResult handle_echo_complex_struct(uint16_t /*client_id*/, uint16_t /*session_id*/,
                                       const std::vector<uint8_t>& input,
                                       std::vector<uint8_t>& output) {
        Deserializer deserializer(input);
        auto sensor_result = deserialize_sensor_reading(deserializer);

        if (sensor_result.is_error()) {
            std::cout << "Failed to deserialize sensor reading" << std::endl;
            return RpcResult::INVALID_PARAMETERS;
        }

        const SensorReading& sensor = sensor_result.get_value();

        std::cout << "Echoing sensor reading: ID=" << (int)sensor.sensor_id
                  << ", Value=" << sensor.value << sensor.unit
                  << ", Timestamp=" << sensor.timestamp << std::endl;

        // Echo back the same data
        output = serialize_sensor_reading(sensor);
        return RpcResult::SUCCESS;
    }
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== SOME/IP Complex Types Server ===" << std::endl;
    std::cout << std::endl;

    ComplexTypesServer server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    server.run();

    return 0;
}
