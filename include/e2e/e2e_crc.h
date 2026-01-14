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

#ifndef E2E_CRC_H
#define E2E_CRC_H

#include <cstdint>
#include <vector>

namespace someip {
namespace e2e {

/**
 * @brief CRC calculation utilities using publicly available standards
 *
 * Implements CRC algorithms from public standards:
 * - SAE-J1850: 8-bit CRC (automotive standard)
 * - ITU-T X.25 / CCITT: 16-bit CRC (telecommunications standard)
 * - CRC32: Standard 32-bit CRC
 */
namespace E2ECRC {

/**
 * @brief Calculate 8-bit CRC using SAE-J1850 algorithm
 *
 * SAE-J1850 is a publicly documented automotive standard.
 * This implementation uses the standard CRC-8 algorithm.
 *
 * @param data Data to calculate CRC for
 * @return 8-bit CRC value
 */
uint8_t calculate_crc8_sae_j1850(const std::vector<uint8_t>& data);

/**
 * @brief Calculate 16-bit CRC using ITU-T X.25 / CCITT algorithm
 *
 * ITU-T Recommendation X.25 (formerly CCITT) is a publicly documented
 * international telecommunications standard.
 * This implementation uses the CCITT polynomial (0x1021).
 *
 * @param data Data to calculate CRC for
 * @return 16-bit CRC value
 */
uint16_t calculate_crc16_itu_x25(const std::vector<uint8_t>& data);

/**
 * @brief Calculate 32-bit CRC using standard CRC32 algorithm
 *
 * Uses the standard CRC-32 polynomial (0x04C11DB7).
 *
 * @param data Data to calculate CRC for
 * @return 32-bit CRC value
 */
uint32_t calculate_crc32(const std::vector<uint8_t>& data);

/**
 * @brief Calculate CRC for a specific range of data
 * @param data Data vector
 * @param offset Start offset
 * @param length Length of data to process
 * @param crc_type 0 = SAE-J1850 (8-bit), 1 = ITU-T X.25 (16-bit), 2 = CRC32
 * @return CRC value (size depends on crc_type)
 */
uint32_t calculate_crc(const std::vector<uint8_t>& data, size_t offset, size_t length, uint8_t crc_type);

} // namespace E2ECRC
} // namespace e2e
} // namespace someip

#endif // E2E_CRC_H
