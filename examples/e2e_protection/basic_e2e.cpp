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
 * @file basic_e2e.cpp
 * @brief Basic E2E protection example
 *
 * Demonstrates:
 * - Enabling E2E protection on a message
 * - Sending protected message
 * - Receiving and validating protected message
 */

#include "e2e/e2e_protection.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "someip/message.h"
#include "common/result.h"
#include <iostream>
#include <vector>

using namespace someip;
using namespace someip::e2e;

int main() {
    // Initialize basic E2E profile (reference implementation)
    initialize_basic_profile();

    // Create a message
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04, 0x05});

    // Configure E2E protection
    E2EConfig config(0x1234);  // Data ID
    config.enable_crc = true;
    config.enable_counter = true;
    config.enable_freshness = true;
    config.crc_type = 1;  // ITU-T X.25 (16-bit CRC)

    // Protect the message
    E2EProtection protection;
    Result result = protection.protect(msg, config);

    if (result != Result::SUCCESS) {
        std::cerr << "Failed to protect message: " << static_cast<int>(result) << std::endl;
        return 1;
    }

    std::cout << "Message protected successfully" << std::endl;
    std::cout << "Message has E2E header: " << (msg.has_e2e_header() ? "yes" : "no") << std::endl;

    // Serialize message
    std::vector<uint8_t> serialized = msg.serialize();
    std::cout << "Serialized message size: " << serialized.size() << " bytes" << std::endl;

    // Deserialize message
    Message received_msg;
    if (!received_msg.deserialize(serialized)) {
        std::cerr << "Failed to deserialize message" << std::endl;
        return 1;
    }

    // Validate E2E protection
    result = protection.validate(received_msg, config);

    if (result == Result::SUCCESS) {
        std::cout << "Message validated successfully" << std::endl;

        auto header_opt = received_msg.get_e2e_header();
        if (header_opt.has_value()) {
            const auto& header = header_opt.value();
            std::cout << "E2E Header:" << std::endl;
            std::cout << "  CRC: 0x" << std::hex << header.crc << std::endl;
            std::cout << "  Counter: " << std::dec << header.counter << std::endl;
            std::cout << "  Data ID: 0x" << std::hex << header.data_id << std::endl;
            std::cout << "  Freshness: 0x" << std::hex << header.freshness_value << std::endl;
        }
    } else {
        std::cerr << "Message validation failed: " << static_cast<int>(result) << std::endl;
        return 1;
    }

    return 0;
}
