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

#ifndef E2E_PROFILE_H
#define E2E_PROFILE_H

#include "e2e_config.h"
#include "e2e_header.h"
#include "someip/message.h"
#include "common/result.h"
#include <cstdint>
#include <string>
#include <memory>

namespace someip {
namespace e2e {

/**
 * @brief Abstract interface for E2E protection profiles
 *
 * This interface allows external E2E profile implementations
 * (e.g., AUTOSAR profiles) to be plugged in via the plugin mechanism.
 */
class E2EProfile {
public:
    virtual ~E2EProfile() = default;

    /**
     * @brief Protect a message before sending
     * @param msg Message to protect
     * @param config E2E configuration
     * @return Result::SUCCESS on success, error code otherwise
     */
    virtual Result protect(Message& msg, const E2EConfig& config) = 0;

    /**
     * @brief Validate a received message
     * @param msg Message to validate
     * @param config E2E configuration
     * @return Result::SUCCESS on success, error code otherwise
     */
    virtual Result validate(const Message& msg, const E2EConfig& config) = 0;

    /**
     * @brief Get the size of the E2E header for this profile
     * @return Header size in bytes
     */
    virtual size_t get_header_size() const = 0;

    /**
     * @brief Get the profile name
     * @return Profile name string
     */
    virtual std::string get_profile_name() const = 0;

    /**
     * @brief Get the profile ID
     * @return Profile ID (unique identifier)
     */
    virtual uint32_t get_profile_id() const = 0;
};

/**
 * @brief Type alias for profile pointer
 */
using E2EProfilePtr = std::unique_ptr<E2EProfile>;

} // namespace e2e
} // namespace someip

#endif // E2E_PROFILE_H
