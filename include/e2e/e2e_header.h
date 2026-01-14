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

#ifndef E2E_HEADER_H
#define E2E_HEADER_H

#include <cstdint>
#include <vector>
#include <optional>

namespace someip {
namespace e2e {

/**
 * @brief E2E protection header structure
 *
 * Represents the E2E header inserted after the Return Code field
 * according to SOME/IP spec feat_req_someip_102 and feat_req_someip_103.
 *
 * The header format is variable size depending on the E2E profile.
 * This structure represents the standard format using public standards.
 */
struct E2EHeader {
    /**
     * @brief CRC value for data integrity checking
     * Uses SAE-J1850 (8-bit) or ITU-T X.25 (16-bit) or CRC32
     */
    uint32_t crc{0};

    /**
     * @brief Sequence counter for replay detection
     * Based on ISO 26262 functional safety concepts
     */
    uint32_t counter{0};

    /**
     * @brief Data ID for identifying the protected data
     */
    uint16_t data_id{0};

    /**
     * @brief Freshness value for stale data detection
     * Based on ISO 26262 functional safety concepts
     */
    uint16_t freshness_value{0};

    /**
     * @brief Default constructor
     */
    E2EHeader() = default;

    /**
     * @brief Constructor with values
     */
    E2EHeader(uint32_t crc_val, uint32_t counter_val, uint16_t data_id_val, uint16_t freshness_val)
        : crc(crc_val), counter(counter_val), data_id(data_id_val), freshness_value(freshness_val) {}

    /**
     * @brief Serialize header to byte vector (big-endian)
     * @return Serialized header bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize header from byte vector (big-endian)
     * @param data Byte vector containing serialized header
     * @param offset Offset into the data vector
     * @return true if successful, false otherwise
     */
    bool deserialize(const std::vector<uint8_t>& data, size_t offset = 0);

    /**
     * @brief Get the size of the header in bytes
     * @return Header size (standard format: 12 bytes)
     */
    static constexpr size_t get_header_size() { return 12; }

    /**
     * @brief Check if header is valid
     * @return true if valid, false otherwise
     */
    bool is_valid() const;
};

} // namespace e2e
} // namespace someip

#endif // E2E_HEADER_H
