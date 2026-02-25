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

#include "e2e/e2e_header.h"
#include "platform/byteorder.h"
#include <cstring>

namespace someip {
namespace e2e {

/**
 * @brief Serialize E2E header to byte vector
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
std::vector<uint8_t> E2EHeader::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(get_header_size());

    // Serialize in big-endian format (network byte order)
    uint32_t crc_be = someip_htonl(crc);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&crc_be),
                reinterpret_cast<const uint8_t*>(&crc_be) + sizeof(uint32_t));

    uint32_t counter_be = someip_htonl(counter);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&counter_be),
                reinterpret_cast<const uint8_t*>(&counter_be) + sizeof(uint32_t));

    uint16_t data_id_be = someip_htons(data_id);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&data_id_be),
                reinterpret_cast<const uint8_t*>(&data_id_be) + sizeof(uint16_t));

    uint16_t freshness_be = someip_htons(freshness_value);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&freshness_be),
                reinterpret_cast<const uint8_t*>(&freshness_be) + sizeof(uint16_t));

    return data;
}

/**
 * @brief Deserialize E2E header from byte vector
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
bool E2EHeader::deserialize(const std::vector<uint8_t>& data, size_t offset) {
    const size_t header_size = get_header_size();
    if (data.size() < offset + header_size) {
        return false;
    }

    uint32_t crc_be;
    std::memcpy(&crc_be, &data[offset], sizeof(uint32_t));
    crc = someip_ntohl(crc_be);
    offset += sizeof(uint32_t);

    uint32_t counter_be;
    std::memcpy(&counter_be, &data[offset], sizeof(uint32_t));
    counter = someip_ntohl(counter_be);
    offset += sizeof(uint32_t);

    uint16_t data_id_be;
    std::memcpy(&data_id_be, &data[offset], sizeof(uint16_t));
    data_id = someip_ntohs(data_id_be);
    offset += sizeof(uint16_t);

    uint16_t freshness_be;
    std::memcpy(&freshness_be, &data[offset], sizeof(uint16_t));
    freshness_value = someip_ntohs(freshness_be);

    return true;
}

bool E2EHeader::is_valid() const {
    // Basic validation - all fields can be any value
    // Specific validation is done by the profile
    return true;
}

} // namespace e2e
} // namespace someip
