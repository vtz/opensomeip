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
 * @file safety_critical.cpp
 * @brief Safety-critical E2E protection example
 *
 * Demonstrates:
 * - E2E protection for safety-critical data
 * - Error handling and recovery
 * - Monitoring and diagnostics
 */

#include "e2e/e2e_protection.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using namespace someip;
using namespace someip::e2e;

int main() {
    // Initialize basic E2E profile (reference implementation)
    initialize_basic_profile();

    // Create safety-critical message (e.g., brake command)
    Message safety_msg(MessageId(0x1000, 0x0001), RequestId(0x0001, 0x0001));
    safety_msg.set_message_type(MessageType::REQUEST_NO_RETURN);

    // Safety-critical payload (example: brake pressure value)
    std::vector<uint8_t> brake_pressure = {0x00, 0x64};  // 100 (example value)
    safety_msg.set_payload(brake_pressure);

    // Configure E2E protection for safety-critical data
    E2EConfig safety_config(0x1000);  // Safety-critical data ID
    safety_config.enable_crc = true;
    safety_config.enable_counter = true;
    safety_config.enable_freshness = true;
    safety_config.crc_type = 1;  // ITU-T X.25 (16-bit CRC) for better error detection
    safety_config.freshness_timeout_ms = 100;  // Strict freshness requirement

    // Protect safety-critical message
    E2EProtection protection;
    Result result = protection.protect(safety_msg, safety_config);

    if (result != Result::SUCCESS) {
        std::cerr << "CRITICAL: Failed to protect safety-critical message!" << std::endl;
        return 1;
    }

    std::cout << "Safety-critical message protected" << std::endl;

    // Simulate transmission and reception
    std::vector<uint8_t> serialized = safety_msg.serialize();

    Message received_msg;
    if (!received_msg.deserialize(serialized)) {
        std::cerr << "CRITICAL: Failed to deserialize safety-critical message!" << std::endl;
        return 1;
    }

    // Validate safety-critical message
    result = protection.validate(received_msg, safety_config);

    if (result == Result::SUCCESS) {
        std::cout << "Safety-critical message validated successfully" << std::endl;
    } else {
        std::cerr << "CRITICAL: Safety-critical message validation failed!" << std::endl;
        std::cerr << "Error code: " << static_cast<int>(result) << std::endl;

        // In safety-critical systems, this would trigger:
        // - Error logging
        // - Fault reporting
        // - Safe state transition
        // - Recovery procedures

        return 1;
    }

    // Demonstrate error detection - corrupt the message
    std::cout << "\nTesting error detection..." << std::endl;
    received_msg.set_payload({0xFF, 0xFF});  // Corrupt payload

    result = protection.validate(received_msg, safety_config);
    if (result != Result::SUCCESS) {
        std::cout << "Error correctly detected: CRC mismatch" << std::endl;
    } else {
        std::cerr << "ERROR: Failed to detect corruption!" << std::endl;
        return 1;
    }

    // Demonstrate freshness timeout
    std::cout << "\nTesting freshness timeout..." << std::endl;
    Message fresh_msg = safety_msg;
    result = protection.protect(fresh_msg, safety_config);

    // Wait longer than freshness timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    result = protection.validate(fresh_msg, safety_config);
    if (result == Result::TIMEOUT) {
        std::cout << "Freshness timeout correctly detected" << std::endl;
    } else {
        std::cout << "Note: Freshness check may have passed (implementation dependent)" << std::endl;
    }

    std::cout << "\nSafety-critical E2E protection demonstration completed" << std::endl;
    return 0;
}
