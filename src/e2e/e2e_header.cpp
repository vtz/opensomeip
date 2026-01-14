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
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace someip {
namespace e2e {

std::vector<uint8_t> E2EHeader::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(get_header_size());

    // Serialize in big-endian format (network byte order)
    uint32_t crc_be = htonl(crc);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&crc_be),
                reinterpret_cast<const uint8_t*>(&crc_be) + sizeof(uint32_t));

    uint32_t counter_be = htonl(counter);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&counter_be),
                reinterpret_cast<const uint8_t*>(&counter_be) + sizeof(uint32_t));

    uint16_t data_id_be = htons(data_id);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&data_id_be),
                reinterpret_cast<const uint8_t*>(&data_id_be) + sizeof(uint16_t));

    uint16_t freshness_be = htons(freshness_value);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&freshness_be),
                reinterpret_cast<const uint8_t*>(&freshness_be) + sizeof(uint16_t));

    return data;
}

bool E2EHeader::deserialize(const std::vector<uint8_t>& data, size_t offset) {
    const size_t header_size = get_header_size();
    if (data.size() < offset + header_size) {
        return false;
    }

    // Deserialize from big-endian format
    uint32_t crc_be = *reinterpret_cast<const uint32_t*>(&data[offset]);
    crc = ntohl(crc_be);
    offset += sizeof(uint32_t);

    uint32_t counter_be = *reinterpret_cast<const uint32_t*>(&data[offset]);
    counter = ntohl(counter_be);
    offset += sizeof(uint32_t);

    uint16_t data_id_be = *reinterpret_cast<const uint16_t*>(&data[offset]);
    data_id = ntohs(data_id_be);
    offset += sizeof(uint16_t);

    uint16_t freshness_be = *reinterpret_cast<const uint16_t*>(&data[offset]);
    freshness_value = ntohs(freshness_be);

    return true;
}

bool E2EHeader::is_valid() const {
    // Basic validation - all fields can be any value
    // Specific validation is done by the profile
    return true;
}

} // namespace e2e
} // namespace someip
