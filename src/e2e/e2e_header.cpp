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

// NOLINTNEXTLINE(misc-include-cleaner) - someip_hton*/someip_ntoh* macros from byteorder_impl.h
#include "platform/byteorder.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace someip::e2e {
// NOLINTBEGIN(misc-include-cleaner) - someip_hton*/someip_ntoh* macros from platform/byteorder.h -> byteorder_impl.h

/**
 * @brief Write the basic 12-byte E2E header in big-endian order
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
void E2EHeader::write_to(uint8_t* out) const {
    if (out == nullptr) {
        return;
    }
    const uint32_t crc_be = someip_htonl(crc);
    std::memcpy(out, &crc_be, sizeof(crc_be));
    const uint32_t counter_be = someip_htonl(counter);
    std::memcpy(out + 4, &counter_be, sizeof(counter_be));
    const uint16_t data_id_be = someip_htons(data_id);
    std::memcpy(out + 8, &data_id_be, sizeof(data_id_be));
    const uint16_t freshness_be = someip_htons(freshness_value);
    std::memcpy(out + 10, &freshness_be, sizeof(freshness_be));
}

bool E2EHeader::read_from(const uint8_t* data, size_t size) {
    if (data == nullptr || size < get_header_size()) {
        return false;
    }
    uint32_t crc_be = 0;
    std::memcpy(&crc_be, data, sizeof(crc_be));
    crc = someip_ntohl(crc_be);
    uint32_t counter_be = 0;
    std::memcpy(&counter_be, data + 4, sizeof(counter_be));
    counter = someip_ntohl(counter_be);
    uint16_t data_id_be = 0;
    std::memcpy(&data_id_be, data + 8, sizeof(data_id_be));
    data_id = someip_ntohs(data_id_be);
    uint16_t freshness_be = 0;
    std::memcpy(&freshness_be, data + 10, sizeof(freshness_be));
    freshness_value = someip_ntohs(freshness_be);
    return true;
}

platform::ByteBuffer E2EHeader::serialize() const {
    platform::ByteBuffer data;
    data.resize(get_header_size());
    if (data.size() != get_header_size() || data.data() == nullptr) {
        return {};
    }
    write_to(data.data());
    return data;
}

/**
 * @brief Deserialize E2E header from byte vector
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
bool E2EHeader::deserialize(const platform::ByteBuffer& data, size_t offset) {
    const size_t header_size = get_header_size();
    if (data.data() == nullptr || header_size > data.size() || offset > data.size() - header_size) {
        return false;
    }
    return read_from(data.data() + offset, data.size() - offset);
}

bool E2EHeader::is_valid() const {
    // Basic validation - all fields can be any value
    // Specific validation is done by the profile
    return true;
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::e2e
