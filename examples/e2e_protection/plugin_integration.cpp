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
 * @file plugin_integration.cpp
 * @brief E2E profile plugin integration example
 *
 * Demonstrates:
 * - Registering an external E2E profile plugin
 * - Using registered profiles
 * - Plugin lifecycle management
 *
 * Note: This example shows the interface; actual AUTOSAR profiles
 * would be provided separately as closed-source plugins.
 */

#include "e2e/e2e_protection.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_profile.h"
#include "e2e/e2e_profile_registry.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "someip/message.h"
#include "common/result.h"
#include <iostream>
#include <memory>

using namespace someip;
using namespace someip::e2e;

/**
 * @brief Example custom E2E profile implementation
 *
 * This is a simple example profile. In practice, AUTOSAR profiles
 * would be implemented as separate closed-source libraries.
 */
class CustomE2EProfile : public E2EProfile {
public:
    Result protect(Message& msg, const E2EConfig& config) override {
        // Simple implementation - just set a basic header
        E2EHeader header(0x12345678, 1, config.data_id, 0x5678);
        msg.set_e2e_header(header);
        return Result::SUCCESS;
    }

    Result validate(const Message& msg, const E2EConfig& config) override {
        auto header_opt = msg.get_e2e_header();
        if (!header_opt.has_value()) {
            return Result::INVALID_ARGUMENT;
        }

        const auto& header = header_opt.value();
        if (header.data_id != config.data_id) {
            return Result::INVALID_ARGUMENT;
        }

        return Result::SUCCESS;
    }

    size_t get_header_size() const override {
        return E2EHeader::get_header_size();
    }

    std::string get_profile_name() const override {
        return "custom";
    }

    uint32_t get_profile_id() const override {
        return 100;  // Custom profile ID
    }
};

int main() {
    // Initialize basic profile (reference implementation)
    initialize_basic_profile();

    // Register custom profile
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    auto custom_profile = std::make_unique<CustomE2EProfile>();

    if (!registry.register_profile(std::move(custom_profile))) {
        std::cerr << "Failed to register custom profile" << std::endl;
        return 1;
    }

    std::cout << "Custom profile registered successfully" << std::endl;

    // Use custom profile
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03});

    E2EConfig config(0x1234);
    config.profile_id = 100;  // Use custom profile
    config.profile_name = "custom";

    E2EProtection protection;
    Result result = protection.protect(msg, config);

    if (result == Result::SUCCESS) {
        std::cout << "Message protected with custom profile" << std::endl;
    } else {
        std::cerr << "Failed to protect message: " << static_cast<int>(result) << std::endl;
        return 1;
    }

    // Validate with custom profile
    result = protection.validate(msg, config);

    if (result == Result::SUCCESS) {
        std::cout << "Message validated with custom profile" << std::endl;
    } else {
        std::cerr << "Validation failed: " << static_cast<int>(result) << std::endl;
        return 1;
    }

    // Unregister custom profile
    if (registry.unregister_profile(100)) {
        std::cout << "Custom profile unregistered successfully" << std::endl;
    }

    return 0;
}
