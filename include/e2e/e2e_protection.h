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

#ifndef E2E_PROTECTION_H
#define E2E_PROTECTION_H

#include "e2e_config.h"
#include "e2e_header.h"
#include "e2e_profile.h"
#include "someip/message.h"
#include "common/result.h"
#include <optional>

namespace someip {
namespace e2e {

/**
 * @brief Main E2E protection manager
 *
 * Provides interface for protecting and validating SOME/IP messages
 * with End-to-End protection according to SOME/IP specification.
 */
class E2EProtection {
public:
    /**
     * @brief Constructor
     */
    E2EProtection() = default;

    /**
     * @brief Destructor
     */
    ~E2EProtection() = default;

    /**
     * @brief Protect a message before sending
     *
     * Adds E2E header to the message according to SOME/IP spec feat_req_someip_102.
     * The header is inserted after the Return Code field.
     *
     * @param message Message to protect
     * @param config E2E configuration
     * @return Result::SUCCESS on success, error code otherwise
     */
    Result protect(Message& message, const E2EConfig& config);

    /**
     * @brief Validate a received message
     *
     * Validates the E2E header and checks CRC, counter, and freshness.
     *
     * @param message Message to validate
     * @param config E2E configuration
     * @return Result::SUCCESS if valid, error code otherwise
     */
    Result validate(const Message& message, const E2EConfig& config);

    /**
     * @brief Extract E2E header from message
     * @param message Message to extract header from
     * @return Optional E2E header if present, empty if not
     */
    std::optional<E2EHeader> extract_header(const Message& message);

    /**
     * @brief Check if message has E2E protection
     * @param message Message to check
     * @return true if E2E header is present, false otherwise
     */
    bool has_e2e_protection(const Message& message) const;
};

} // namespace e2e
} // namespace someip

#endif // E2E_PROTECTION_H
