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
 * @brief Events Subscriber Example
 *
 * This example demonstrates the publish-subscribe pattern in SOME/IP:
 * - Subscriber receives temperature and speed events
 * - Subscribes to specific events from the publisher
 * - Processes and displays received event data
 *
 * This demonstrates the fundamental event subscription pattern.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iomanip>
#include <cstring>

#include <events/event_subscriber.h>
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

class SensorSubscriber : public EventSubscriber {
public:
    SensorSubscriber() : subscriber_(SENSOR_SERVICE_ID) {}

    bool initialize() {
        // Set up event handlers
        subscriber_.set_event_handler(TEMPERATURE_EVENT_ID,
            [this](const EventNotification& notification) {
                on_temperature_event(notification);
            });

        subscriber_.set_event_handler(SPEED_EVENT_ID,
            [this](const EventNotification& notification) {
                on_speed_event(notification);
            });

        // Subscribe to events
        if (!subscriber_.subscribe_event(TEMPERATURE_EVENT_ID, SENSOR_EVENTGROUP_ID)) {
            std::cerr << "Failed to subscribe to temperature event" << std::endl;
            return false;
        }

        if (!subscriber_.subscribe_event(SPEED_EVENT_ID, SENSOR_EVENTGROUP_ID)) {
            std::cerr << "Failed to subscribe to speed event" << std::endl;
            return false;
        }

        if (!subscriber_.initialize()) {
            std::cerr << "Failed to initialize event subscriber" << std::endl;
            return false;
        }

        std::cout << "Sensor Subscriber initialized for service 0x" << std::hex << SENSOR_SERVICE_ID << std::endl;
        std::cout << "Subscribed to events:" << std::endl;
        std::cout << "  - Temperature (ID: 0x" << std::hex << TEMPERATURE_EVENT_ID << ")" << std::endl;
        std::cout << "  - Speed (ID: 0x" << std::hex << SPEED_EVENT_ID << ")" << std::endl;

        return true;
    }

    void run() {
        std::cout << "\nSensor Subscriber running. Press Ctrl+C to exit." << std::endl;
        std::cout << "Waiting for sensor events..." << std::endl;

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        subscriber_.shutdown();
        std::cout << "Sensor Subscriber shut down." << std::endl;
    }

private:
    EventSubscriber subscriber_;

    void on_temperature_event(const EventNotification& notification) {
        if (notification.event_data.size() < 4) {
            std::cout << "❌ Temperature event: Invalid data size" << std::endl;
            return;
        }

        // Deserialize temperature (big-endian float)
        uint32_t temp_bits = (notification.event_data[0] << 24) |
                           (notification.event_data[1] << 16) |
                           (notification.event_data[2] << 8) |
                           notification.event_data[3];

        float temperature;
        std::memcpy(&temperature, &temp_bits, sizeof(float));

        // Get timestamp
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            notification.timestamp.time_since_epoch()).count();

        std::cout << "🌡️  Temperature Event: " << std::fixed << std::setprecision(1)
                  << temperature << "°C (at " << timestamp << "ms)" << std::endl;
    }

    void on_speed_event(const EventNotification& notification) {
        if (notification.event_data.size() < 4) {
            std::cout << "❌ Speed event: Invalid data size" << std::endl;
            return;
        }

        // Deserialize speed (big-endian float)
        uint32_t speed_bits = (notification.event_data[0] << 24) |
                            (notification.event_data[1] << 16) |
                            (notification.event_data[2] << 8) |
                            notification.event_data[3];

        float speed;
        std::memcpy(&speed, &speed_bits, sizeof(float));

        // Get timestamp
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            notification.timestamp.time_since_epoch()).count();

        std::cout << "🚗 Speed Event: " << std::fixed << std::setprecision(1)
                  << speed << " km/h (at " << timestamp << "ms)" << std::endl;
    }
};

int main() {
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== SOME/IP Events Subscriber ===" << std::endl;
    std::cout << std::endl;

    SensorSubscriber subscriber;

    if (!subscriber.initialize()) {
        std::cerr << "Failed to initialize subscriber" << std::endl;
        return 1;
    }

    subscriber.run();

    return 0;
}
