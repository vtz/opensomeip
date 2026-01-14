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

#include "e2e/e2e_protection.h"
#include "e2e/e2e_profile_registry.h"
#include "e2e/e2e_header.h"
#include "someip/message.h"
#include "common/result.h"

namespace someip {
namespace e2e {

Result E2EProtection::protect(Message& message, const E2EConfig& config) {
    // Get profile from registry
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    E2EProfile* profile = registry.get_profile(config.profile_id);

    // If profile not found by ID, try by name
    if (!profile) {
        profile = registry.get_profile(config.profile_name);
    }

    // If still not found, use default profile
    if (!profile) {
        profile = registry.get_default_profile();
    }

    if (!profile) {
        return Result::NOT_INITIALIZED;  // Basic profile not initialized
    }

    // Call profile's protect method
    return profile->protect(message, config);
}

Result E2EProtection::validate(const Message& message, const E2EConfig& config) {
    // Get profile from registry
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    E2EProfile* profile = registry.get_profile(config.profile_id);

    // If profile not found by ID, try by name
    if (!profile) {
        profile = registry.get_profile(config.profile_name);
    }

    // If still not found, use default profile
    if (!profile) {
        profile = registry.get_default_profile();
    }

    if (!profile) {
        return Result::NOT_INITIALIZED;  // Basic profile not initialized
    }

    // Call profile's validate method
    // Note: validate is const, but we need non-const message for some operations
    // Create a mutable copy for validation
    Message msg_copy = message;
    return profile->validate(msg_copy, config);
}

std::optional<E2EHeader> E2EProtection::extract_header(const Message& message) {
    return message.get_e2e_header();
}

bool E2EProtection::has_e2e_protection(const Message& message) const {
    return message.has_e2e_header();
}

} // namespace e2e
} // namespace someip
