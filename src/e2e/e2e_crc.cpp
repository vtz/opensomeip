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

#include "e2e/e2e_crc.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief E2E CRC calculation functions
 * @satisfies feat_req_someip_102
 *
 * Provides CRC calculation using publicly available standards:
 * - SAE-J1850 (8-bit)
 * - ITU-T X.25 (16-bit)
 * - ISO 3309 / IEEE 802.3 (32-bit)
 */
namespace someip::e2e::E2ECRC {

// SAE-J1850 CRC-8 polynomial: 0x1D (x^8 + x^4 + x^3 + x^2 + 1)
static constexpr uint8_t SAE_J1850_POLY = 0x1D;
static constexpr uint8_t SAE_J1850_INIT = 0xFF;

/** @implements REQ_E2E_PLUGIN_004 */
uint8_t calculate_crc8_sae_j1850(const std::vector<uint8_t>& data) {
    uint32_t crc_reg = SAE_J1850_INIT;

    for (const uint8_t byte : data) {
        crc_reg ^= static_cast<uint32_t>(byte);
        for (int i = 0; i < 8; ++i) {
            if ((crc_reg & 0x80U) != 0) {
                crc_reg = ((crc_reg << 1U) ^ static_cast<uint32_t>(SAE_J1850_POLY)) & 0xFFU;
            } else {
                crc_reg = (crc_reg << 1U) & 0xFFU;
            }
        }
    }

    return static_cast<uint8_t>(crc_reg);
}

// ITU-T X.25 / CCITT CRC-16 polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
static constexpr uint16_t ITU_X25_POLY = 0x1021;
static constexpr uint16_t ITU_X25_INIT = 0xFFFF;

uint16_t calculate_crc16_itu_x25(const std::vector<uint8_t>& data) {
    uint32_t crc_reg = ITU_X25_INIT;

    for (const uint8_t byte : data) {
        crc_reg ^= static_cast<uint32_t>(byte) << 8U;
        for (int i = 0; i < 8; ++i) {
            if ((crc_reg & 0x8000U) != 0) {
                crc_reg = ((crc_reg << 1U) ^ static_cast<uint32_t>(ITU_X25_POLY)) & 0xFFFFU;
            } else {
                crc_reg = (crc_reg << 1U) & 0xFFFFU;
            }
        }
    }

    return static_cast<uint16_t>(crc_reg);
}

// CRC-32 polynomial: 0x04C11DB7 (IEEE 802.3)
static constexpr uint32_t CRC32_POLY = 0x04C11DB7;
static constexpr uint32_t CRC32_INIT = 0xFFFFFFFF;

namespace {

const std::array<uint32_t, 256>& get_crc32_table() {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i << 24U;
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x80000000U) {
                    crc = (crc << 1U) ^ CRC32_POLY;
                } else {
                    crc <<= 1U;
                }
            }
            t[i] = crc;
        }
        return t;
    }();
    return table;
}

}  // namespace

uint32_t calculate_crc32(const std::vector<uint8_t>& data) {
    const auto& crc32_table = get_crc32_table();

    uint32_t crc = CRC32_INIT;

    for (const uint8_t byte : data) {
        const uint32_t index = ((crc >> 24U) ^ static_cast<uint32_t>(byte)) & 0xFFU;
        crc = (crc << 8U) ^ crc32_table[index];
    }

    return crc;
}

std::optional<uint32_t> calculate_crc(const std::vector<uint8_t>& data, size_t offset, size_t length, uint8_t crc_type) {
    if (offset > data.size() || length > data.size() || offset > data.size() - length ||
        offset > static_cast<size_t>(PTRDIFF_MAX) || length > static_cast<size_t>(PTRDIFF_MAX)) {
        return std::nullopt;
    }

    auto first = data.begin() + static_cast<std::ptrdiff_t>(offset);
    const std::vector<uint8_t> slice(first, first + static_cast<std::ptrdiff_t>(length));

    switch (crc_type) {
        case 0:  // SAE-J1850 (8-bit)
            return calculate_crc8_sae_j1850(slice);
        case 1:  // ITU-T X.25 (16-bit)
            return calculate_crc16_itu_x25(slice);
        case 2:  // CRC32
            return calculate_crc32(slice);
        default:
            return std::nullopt;
    }
}

}  // namespace someip::e2e::E2ECRC
