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
 * @brief Events Publisher Example
 *
 * This example demonstrates the publish-subscribe pattern in SOME/IP:
 * - Publisher sends temperature and speed events
 * - Events are sent periodically to simulate sensor data
 * - Shows how to publish different types of event data
 *
 * This demonstrates the fundamental event publishing pattern.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <random>
#include <iomanip>
#include <cstring>

#include <events/event_publisher.h>
#include <events/event_types.h>

using namespace someip;
using namespace someip::events;

// Service and event IDs
const uint16_t SENSOR_SERVICE_ID = 0x3000;
const uint16_t TEMPERATURE_EVENT_ID = 0x8001;
const uint16_t SPEED_EVENT_ID = 0x8002;

// Event group for sensor events
const uint16_t SENSOR_EVENTGROUP_ID = 0x0001;

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

class SensorPublisher {
public:
    SensorPublisher() : publisher_(SENSOR_SERVICE_ID, 0x0001) {}

    bool initialize() {
        // Create event configurations
        EventConfig temp_config;
        temp_config.event_id = TEMPERATURE_EVENT_ID;
        temp_config.eventgroup_id = SENSOR_EVENTGROUP_ID;
        temp_config.reliability = Reliability::UNRELIABLE;
        temp_config.notification_type = NotificationType::PERIODIC;
        temp_config.cycle_time = std::chrono::milliseconds(2000);  // Every 2 seconds

        EventConfig speed_config;
        speed_config.event_id = SPEED_EVENT_ID;
        speed_config.eventgroup_id = SENSOR_EVENTGROUP_ID;
        speed_config.reliability = Reliability::UNRELIABLE;
        speed_config.notification_type = NotificationType::PERIODIC;
        speed_config.cycle_time = std::chrono::milliseconds(1500);  // Every 1.5 seconds

        // Register the events
        if (!publisher_.register_event(temp_config)) {
            std::cerr << "Failed to register temperature event" << std::endl;
            return false;
        }

        if (!publisher_.register_event(speed_config)) {
            std::cerr << "Failed to register speed event" << std::endl;
            return false;
        }

        if (!publisher_.initialize()) {
            std::cerr << "Failed to initialize event publisher" << std::endl;
            return false;
        }

        std::cout << "Sensor Publisher initialized for service 0x" << std::hex << SENSOR_SERVICE_ID << std::endl;
        std::cout << "Publishing events:" << std::endl;
        std::cout << "  - Temperature (ID: 0x" << std::hex << TEMPERATURE_EVENT_ID << ") every 2 seconds" << std::endl;
        std::cout << "  - Speed (ID: 0x" << std::hex << SPEED_EVENT_ID << ") every 1.5 seconds" << std::endl;

        return true;
    }

    void run() {
        std::cout << "\nSensor Publisher running. Press Ctrl+C to exit." << std::endl;
        std::cout << "Publishing sensor data..." << std::endl;

        // Initialize random number generator for realistic sensor data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> temp_dist(15.0, 35.0);  // Temperature: 15-35°C
        std::uniform_real_distribution<> speed_dist(0.0, 120.0); // Speed: 0-120 km/h

        auto last_temp_time = std::chrono::steady_clock::now();
        auto last_speed_time = std::chrono::steady_clock::now();

        while (running) {
            auto now = std::chrono::steady_clock::now();

            // Publish temperature event every 2 seconds
            if (now - last_temp_time >= std::chrono::milliseconds(2000)) {
                float temperature = temp_dist(gen);

                // Serialize temperature (big-endian float)
                uint32_t temp_bits;
                std::memcpy(&temp_bits, &temperature, sizeof(float));

                std::vector<uint8_t> temp_data(4);
                temp_data[0] = (temp_bits >> 24) & 0xFF;
                temp_data[1] = (temp_bits >> 16) & 0xFF;
                temp_data[2] = (temp_bits >> 8) & 0xFF;
                temp_data[3] = temp_bits & 0xFF;

                if (publisher_.publish_event(TEMPERATURE_EVENT_ID, temp_data)) {
                    std::cout << "📊 Published Temperature: " << std::fixed << std::setprecision(1)
                              << temperature << "°C" << std::endl;
                } else {
                    std::cout << "❌ Failed to publish temperature event" << std::endl;
                }

                last_temp_time = now;
            }

            // Publish speed event every 1.5 seconds
            if (now - last_speed_time >= std::chrono::milliseconds(1500)) {
                float speed = speed_dist(gen);

                // Serialize speed (big-endian float)
                uint32_t speed_bits;
                std::memcpy(&speed_bits, &speed, sizeof(float));

                std::vector<uint8_t> speed_data(4);
                speed_data[0] = (speed_bits >> 24) & 0xFF;
                speed_data[1] = (speed_bits >> 16) & 0xFF;
                speed_data[2] = (speed_bits >> 8) & 0xFF;
                speed_data[3] = speed_bits & 0xFF;

                if (publisher_.publish_event(SPEED_EVENT_ID, speed_data)) {
                    std::cout << "🚗 Published Speed: " << std::fixed << std::setprecision(1)
                              << speed << " km/h" << std::endl;
                } else {
                    std::cout << "❌ Failed to publish speed event" << std::endl;
                }

                last_speed_time = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        publisher_.shutdown();
        std::cout << "Sensor Publisher shut down." << std::endl;
    }

private:
    EventPublisher publisher_;
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== SOME/IP Events Publisher ===" << std::endl;
    std::cout << std::endl;

    SensorPublisher publisher;

    if (!publisher.initialize()) {
        std::cerr << "Failed to initialize publisher" << std::endl;
        return 1;
    }

    publisher.run();

    return 0;
}
