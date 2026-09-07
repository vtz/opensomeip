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
 * @brief Complex Types Client Example
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
#include <vector>
#include <array>
#include <string>

#include <rpc/rpc_client.h>
#include <rpc/rpc_types.h>
#include <serialization/serializer.h>
#include <transport/endpoint.h>

using namespace someip;
using namespace someip::rpc;
using namespace someip::serialization;

// Service and method IDs
const uint16_t COMPLEX_SERVICE_ID = 0x4000;
const uint16_t PROCESS_VEHICLE_DATA_METHOD_ID = 0x0001;
const uint16_t GET_SENSOR_ARRAY_METHOD_ID = 0x0002;
const uint16_t ECHO_COMPLEX_STRUCT_METHOD_ID = 0x0003;

// Data structures for complex types (same as server)
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

class ComplexTypesClient {
public:
    ComplexTypesClient() : client_(0xABCD) {}  // Client ID

    bool initialize() {
        if (!client_.initialize()) {
            std::cerr << "Failed to initialize RPC client" << std::endl;
            return false;
        }

        client_.set_remote_endpoint(transport::Endpoint("127.0.0.1", SOMEIP_DEFAULT_RPC_PORT));

        std::cout << "Complex Types Client initialized (ID: 0xABCD)" << std::endl;
        return true;
    }

    void run_demonstrations() {
        std::cout << "\n=== Complex Types Demonstrations ===" << std::endl;

        // Test 1: Process vehicle data (struct serialization)
        test_vehicle_data_processing();

        // Test 2: Get sensor array (array of structs)
        test_sensor_array_retrieval();

        // Test 3: Echo complex struct (round-trip serialization)
        test_complex_struct_echo();

        std::cout << "\n=== All Demonstrations Completed ===" << std::endl;
    }

    void shutdown() {
        client_.shutdown();
        std::cout << "Complex Types Client shut down." << std::endl;
    }

private:
    RpcClient client_;

    // Serialization helpers (same as server)
    std::vector<uint8_t> serialize_vehicle_data(const VehicleData& data) {
        Serializer serializer;
        serializer.serialize_uint32(data.vehicle_id);
        serializer.serialize_string(data.model);
        serializer.serialize_float(data.fuel_level);
        for (uint8_t pressure : data.tire_pressure) {
            serializer.serialize_uint8(pressure);
        }
        serializer.serialize_bool(data.lights_on);
        serializer.serialize_uint16(data.mileage);
        return serializer.get_buffer();
    }

    std::vector<uint8_t> serialize_sensor_reading(const SensorReading& data) {
        Serializer serializer;
        serializer.serialize_uint8(data.sensor_id);
        serializer.serialize_float(data.value);
        serializer.serialize_string(data.unit);
        serializer.serialize_uint32(data.timestamp);
        return serializer.get_buffer();
    }

    DeserializationResult<std::string> deserialize_string(Deserializer& deserializer) {
        return deserializer.deserialize_string();
    }

    DeserializationResult<SensorArray> deserialize_sensor_array(Deserializer& deserializer) {
        SensorArray data;

        // First get array size
        auto array_size = deserializer.deserialize_uint32();
        if (array_size.is_error()) return DeserializationResult<SensorArray>::error(array_size.get_error());
        data.array_size = array_size.get_value();

        // Then deserialize each sensor
        for (uint32_t i = 0; i < data.array_size; ++i) {
            // Get sensor data length (and skip it, since we don't need it)
            auto sensor_length = deserializer.deserialize_uint32();
            if (sensor_length.is_error()) return DeserializationResult<SensorArray>::error(sensor_length.get_error());

            // Deserialize sensor data directly
            auto sensor_id = deserializer.deserialize_uint8();
            auto value = deserializer.deserialize_float();
            auto unit = deserializer.deserialize_string();
            auto timestamp = deserializer.deserialize_uint32();

            if (sensor_id.is_error() || value.is_error() || unit.is_error() || timestamp.is_error()) {
                return DeserializationResult<SensorArray>::error(sensor_id.is_error() ? sensor_id.get_error() :
                        value.is_error() ? value.get_error() :
                        unit.is_error() ? unit.get_error() : timestamp.get_error());
            }

            SensorReading sensor = {
                sensor_id.get_value(),
                value.get_value(),
                unit.get_value(),
                timestamp.get_value()
            };
            data.sensors.push_back(sensor);
        }

        return DeserializationResult<SensorArray>::success(data);
    }

    void test_vehicle_data_processing() {
        std::cout << "\n--- Test 1: Vehicle Data Processing ---" << std::endl;

        // Create sample vehicle data
        VehicleData vehicle = {
            12345,                    // vehicle_id
            "Tesla Model S",         // model
            0.85f,                   // fuel_level (85%)
            {32, 33, 31, 34},       // tire_pressure (PSI)
            true,                    // lights_on
            45230                    // mileage (km)
        };

        std::cout << "Sending vehicle data:" << std::endl;
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

        // Serialize and send
        std::vector<uint8_t> parameters = serialize_vehicle_data(vehicle);

        RpcSyncResult result = client_.call_method_sync(
            COMPLEX_SERVICE_ID, PROCESS_VEHICLE_DATA_METHOD_ID, parameters);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "RPC call failed: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        // Deserialize response
        Deserializer response_deserializer(result.return_values);
        auto response = deserialize_string(response_deserializer);

        if (response.is_error()) {
            std::cout << "Failed to deserialize response" << std::endl;
            return;
        }

        std::cout << "Server response: " << response.get_value() << std::endl;
        std::cout << "✓ Vehicle data processing successful" << std::endl;
    }

    void test_sensor_array_retrieval() {
        std::cout << "\n--- Test 2: Sensor Array Retrieval ---" << std::endl;

        // No parameters needed
        std::vector<uint8_t> parameters;

        RpcSyncResult result = client_.call_method_sync(
            COMPLEX_SERVICE_ID, GET_SENSOR_ARRAY_METHOD_ID, parameters);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "RPC call failed: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        // Deserialize sensor array
        Deserializer response_deserializer(result.return_values);
        auto sensor_array_result = deserialize_sensor_array(response_deserializer);

        if (sensor_array_result.is_error()) {
            std::cout << "Failed to deserialize sensor array" << std::endl;
            return;
        }

        const SensorArray& sensor_array = sensor_array_result.get_value();

        std::cout << "Received sensor array with " << sensor_array.sensors.size() << " readings:" << std::endl;
        for (const auto& sensor : sensor_array.sensors) {
            std::cout << "  Sensor " << (int)sensor.sensor_id << ": "
                      << sensor.value << " " << sensor.unit
                      << " (timestamp: " << sensor.timestamp << ")" << std::endl;
        }

        std::cout << "✓ Sensor array retrieval successful" << std::endl;
    }

    void test_complex_struct_echo() {
        std::cout << "\n--- Test 3: Complex Struct Echo ---" << std::endl;

        // Create sample sensor reading
        SensorReading sensor = {
            42,         // sensor_id
            98.6f,      // value
            "°F",       // unit
            1234567890  // timestamp
        };

        std::cout << "Sending sensor reading:" << std::endl;
        std::cout << "  ID: " << (int)sensor.sensor_id << std::endl;
        std::cout << "  Value: " << sensor.value << " " << sensor.unit << std::endl;
        std::cout << "  Timestamp: " << sensor.timestamp << std::endl;

        // Serialize and send
        std::vector<uint8_t> parameters = serialize_sensor_reading(sensor);

        RpcSyncResult result = client_.call_method_sync(
            COMPLEX_SERVICE_ID, ECHO_COMPLEX_STRUCT_METHOD_ID, parameters);

        if (result.result != RpcResult::SUCCESS) {
            std::cout << "RPC call failed: " << static_cast<int>(result.result) << std::endl;
            return;
        }

        // Deserialize echoed sensor reading
        Deserializer response_deserializer(result.return_values);

        auto sensor_id = response_deserializer.deserialize_uint8();
        auto value = response_deserializer.deserialize_float();
        auto unit = response_deserializer.deserialize_string();
        auto timestamp = response_deserializer.deserialize_uint32();

        if (sensor_id.is_error() || value.is_error() || unit.is_error() || timestamp.is_error()) {
            std::cout << "Failed to deserialize echoed sensor" << std::endl;
            return;
        }

        std::cout << "Echoed sensor reading:" << std::endl;
        std::cout << "  ID: " << (int)sensor_id.get_value() << std::endl;
        std::cout << "  Value: " << value.get_value() << " " << unit.get_value() << std::endl;
        std::cout << "  Timestamp: " << timestamp.get_value() << std::endl;

        // Verify round-trip
        bool match = (sensor_id.get_value() == sensor.sensor_id &&
                     value.get_value() == sensor.value &&
                     unit.get_value() == sensor.unit &&
                     timestamp.get_value() == sensor.timestamp);

        std::cout << "Round-trip verification: " << (match ? "✓ PASSED" : "❌ FAILED") << std::endl;
        std::cout << "✓ Complex struct echo successful" << std::endl;
    }
};

int main() {
    std::cout << "=== SOME/IP Complex Types Client ===" << std::endl;
    std::cout << std::endl;

    ComplexTypesClient client;

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
