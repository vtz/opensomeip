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
#include <algorithm>

namespace someip {
namespace e2e {

/**
 * @brief E2E CRC calculation functions
 * @implements REQ_E2E_PLUGIN_004
 * @satisfies feat_req_someip_102
 *
 * Provides CRC calculation using publicly available standards:
 * - SAE-J1850 (8-bit)
 * - ITU-T X.25 (16-bit)
 * - ISO 3309 / IEEE 802.3 (32-bit)
 */
namespace E2ECRC {

// SAE-J1850 CRC-8 polynomial: 0x1D (x^8 + x^4 + x^3 + x^2 + 1)
static constexpr uint8_t SAE_J1850_POLY = 0x1D;
static constexpr uint8_t SAE_J1850_INIT = 0xFF;

uint8_t calculate_crc8_sae_j1850(const std::vector<uint8_t>& data) {
    uint8_t crc = SAE_J1850_INIT;

    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ SAE_J1850_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

// ITU-T X.25 / CCITT CRC-16 polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
static constexpr uint16_t ITU_X25_POLY = 0x1021;
static constexpr uint16_t ITU_X25_INIT = 0xFFFF;

uint16_t calculate_crc16_itu_x25(const std::vector<uint8_t>& data) {
    uint16_t crc = ITU_X25_INIT;

    for (uint8_t byte : data) {
        crc ^= (static_cast<uint16_t>(byte) << 8);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ ITU_X25_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

// CRC-32 polynomial: 0x04C11DB7 (IEEE 802.3)
static constexpr uint32_t CRC32_POLY = 0x04C11DB7;
static constexpr uint32_t CRC32_INIT = 0xFFFFFFFF;

// Precomputed CRC32 lookup table
static uint32_t crc32_table[256];

// Initialize CRC32 lookup table (called once)
static bool crc32_table_initialized = false;

static void init_crc32_table() {
    if (crc32_table_initialized) {
        return;
    }

    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i << 24;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ CRC32_POLY;
            } else {
                crc <<= 1;
            }
        }
        crc32_table[i] = crc;
    }

    crc32_table_initialized = true;
}

uint32_t calculate_crc32(const std::vector<uint8_t>& data) {
    init_crc32_table();

    uint32_t crc = CRC32_INIT;

    for (uint8_t byte : data) {
        uint32_t index = ((crc >> 24) ^ byte) & 0xFF;
        crc = (crc << 8) ^ crc32_table[index];
    }

    return crc;
}

uint32_t calculate_crc(const std::vector<uint8_t>& data, size_t offset, size_t length, uint8_t crc_type) {
    if (offset + length > data.size()) {
        return 0;
    }

    std::vector<uint8_t> slice(data.begin() + offset, data.begin() + offset + length);

    switch (crc_type) {
        case 0:  // SAE-J1850 (8-bit)
            return calculate_crc8_sae_j1850(slice);
        case 1:  // ITU-T X.25 (16-bit)
            return calculate_crc16_itu_x25(slice);
        case 2:  // CRC32
            return calculate_crc32(slice);
        default:
            return 0;
    }
}

} // namespace E2ECRC
} // namespace e2e
} // namespace someip
