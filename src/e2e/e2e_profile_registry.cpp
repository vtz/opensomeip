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

#include "e2e/e2e_profile_registry.h"
#include <algorithm>

namespace someip {
namespace e2e {

/**
 * @brief Get singleton instance of the E2E profile registry
 * @implements REQ_E2E_PLUGIN_002
 */
E2EProfileRegistry& E2EProfileRegistry::instance() {
    static E2EProfileRegistry registry;
    return registry;
}

/**
 * @brief Register an E2E profile plugin
 * @implements REQ_E2E_PLUGIN_002
 * @implements REQ_E2E_PLUGIN_003
 */
bool E2EProfileRegistry::register_profile(E2EProfilePtr profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t profile_id = profile->get_profile_id();
    std::string profile_name = profile->get_profile_name();

    // Check if profile ID already exists
    if (profiles_by_id_.find(profile_id) != profiles_by_id_.end()) {
        return false;
    }

    // Check if profile name already exists
    if (profiles_by_name_.find(profile_name) != profiles_by_name_.end()) {
        return false;
    }

    // Register profile
    E2EProfile* raw_ptr = profile.get();
    profiles_by_id_[profile_id] = std::move(profile);
    profiles_by_name_[profile_name] = raw_ptr;

    return true;
}

/**
 * @brief Get an E2E profile by ID
 * @implements REQ_E2E_PLUGIN_002
 */
E2EProfile* E2EProfileRegistry::get_profile(uint32_t profile_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = profiles_by_id_.find(profile_id);
    if (it != profiles_by_id_.end()) {
        return it->second.get();
    }

    return nullptr;
}

E2EProfile* E2EProfileRegistry::get_profile(const std::string& profile_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = profiles_by_name_.find(profile_name);
    if (it != profiles_by_name_.end()) {
        return it->second;
    }

    return nullptr;
}

bool E2EProfileRegistry::unregister_profile(uint32_t profile_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = profiles_by_id_.find(profile_id);
    if (it == profiles_by_id_.end()) {
        return false;
    }

    // Remove from name map
    std::string profile_name = it->second->get_profile_name();
    profiles_by_name_.erase(profile_name);

    // Remove from ID map
    profiles_by_id_.erase(it);

    return true;
}

bool E2EProfileRegistry::is_registered(uint32_t profile_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profiles_by_id_.find(profile_id) != profiles_by_id_.end();
}

E2EProfile* E2EProfileRegistry::get_default_profile() {
    // Default profile has ID 0 and name "standard"
    return get_profile(0);
}

} // namespace e2e
} // namespace someip
