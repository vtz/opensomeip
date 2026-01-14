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

#ifndef E2E_PROFILE_REGISTRY_H
#define E2E_PROFILE_REGISTRY_H

#include "e2e_profile.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace someip {
namespace e2e {

/**
 * @brief Registry for E2E protection profiles
 *
 * Manages registration and lookup of E2E protection profiles.
 * Allows external profiles (e.g., AUTOSAR) to be plugged in.
 */
class E2EProfileRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry instance
     */
    static E2EProfileRegistry& instance();

    /**
     * @brief Register an E2E profile
     * @param profile Unique pointer to the profile to register
     * @return true if registration successful, false if profile ID/name already exists
     */
    bool register_profile(E2EProfilePtr profile);

    /**
     * @brief Get a profile by ID
     * @param profile_id Profile ID
     * @return Pointer to profile or nullptr if not found
     */
    E2EProfile* get_profile(uint32_t profile_id);

    /**
     * @brief Get a profile by name
     * @param profile_name Profile name
     * @return Pointer to profile or nullptr if not found
     */
    E2EProfile* get_profile(const std::string& profile_name);

    /**
     * @brief Unregister a profile by ID
     * @param profile_id Profile ID to unregister
     * @return true if unregistered, false if not found
     */
    bool unregister_profile(uint32_t profile_id);

    /**
     * @brief Check if a profile is registered
     * @param profile_id Profile ID
     * @return true if registered, false otherwise
     */
    bool is_registered(uint32_t profile_id) const;

    /**
     * @brief Get default profile (basic profile)
     * @return Pointer to default profile or nullptr if not available
     */
    E2EProfile* get_default_profile();

private:
    E2EProfileRegistry() = default;
    ~E2EProfileRegistry() = default;
    E2EProfileRegistry(const E2EProfileRegistry&) = delete;
    E2EProfileRegistry& operator=(const E2EProfileRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, E2EProfilePtr> profiles_by_id_;
    std::unordered_map<std::string, E2EProfile*> profiles_by_name_;
};

} // namespace e2e
} // namespace someip

#endif // E2E_PROFILE_REGISTRY_H
