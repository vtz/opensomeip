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

#ifndef E2E_CONFIG_H
#define E2E_CONFIG_H

#include <cstdint>
#include <string>

namespace someip {
namespace e2e {

/**
 * @brief E2E protection configuration
 *
 * Configuration for End-to-End protection of SOME/IP messages.
 * Based on ISO 26262 functional safety concepts and public standards.
 */
struct E2EConfig {
    /**
     * @brief Profile identifier (0 = basic profile, others for plugins)
     */
    uint32_t profile_id{0};

    /**
     * @brief Profile name (e.g., "standard", "autosar_c", etc.)
     */
    std::string profile_name{"standard"};

    /**
     * @brief Data ID for identifying the protected data
     */
    uint16_t data_id{0};

    /**
     * @brief Offset from Return Code field (default 64 bits = 8 bytes)
     * According to SOME/IP spec feat_req_someip_102
     */
    uint32_t offset{8};

    /**
     * @brief Enable CRC calculation
     */
    bool enable_crc{true};

    /**
     * @brief Enable counter mechanism for replay detection
     */
    bool enable_counter{true};

    /**
     * @brief Enable freshness value for stale data detection
     */
    bool enable_freshness{true};

    /**
     * @brief Maximum counter value before rollover
     */
    uint32_t max_counter_value{0xFFFFFFFF};

    /**
     * @brief Freshness timeout in milliseconds
     */
    uint32_t freshness_timeout_ms{1000};

    /**
     * @brief CRC type: 0 = SAE-J1850 (8-bit), 1 = ITU-T X.25 (16-bit), 2 = CRC32
     */
    uint8_t crc_type{1};  // Default to 16-bit ITU-T X.25

    /**
     * @brief Default constructor
     */
    E2EConfig() = default;

    /**
     * @brief Constructor with data ID
     */
    explicit E2EConfig(uint16_t data_id) : data_id(data_id) {}
};

} // namespace e2e
} // namespace someip

#endif // E2E_CONFIG_H
