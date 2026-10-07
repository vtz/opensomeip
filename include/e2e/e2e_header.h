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

#include "platform/buffer_pool.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace someip::e2e {

/**
 * @brief E2E protection header structure
 *
 * The shipped basic profile is this 12-byte layout (CRC, counter, Data ID,
 * freshness). Message also stores arbitrary profile bytes up to
 * SOMEIP_MAX_E2E_HEADER_SIZE; get_e2e_header() returns this struct only when
 * the stored header is exactly 12 bytes. Placement is E2EConfig::offset_bits,
 * not a field of this struct (feat_req_someip_102 / feat_req_someip_103).
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
    platform::ByteBuffer serialize() const;

    /**
     * @brief Write the 12-byte header to @p out (big-endian).
     * @param out At least get_header_size() bytes. No-op if null.
     */
    void write_to(uint8_t* out) const;

    /**
     * @brief Read a 12-byte header from raw bytes (big-endian).
     * @return false if @p data is null or shorter than get_header_size()
     */
    bool read_from(const uint8_t* data, size_t size);

    /**
     * @brief Deserialize header from byte vector (big-endian)
     * @param data Byte vector containing serialized header
     * @param offset Offset into the data vector
     * @return true if successful, false otherwise
     */
    bool deserialize(const platform::ByteBuffer& data, size_t offset = 0);

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

}  // namespace someip::e2e

#endif // E2E_HEADER_H
