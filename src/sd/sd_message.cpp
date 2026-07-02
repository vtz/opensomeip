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

#include "sd/sd_message.h"

#include "sd/sd_types.h"
// NOLINTNEXTLINE(misc-include-cleaner) - someip_hton*/someip_ntoh* macros from byteorder_impl.h
#include "platform/byteorder.h"
// NOLINTNEXTLINE(misc-include-cleaner) - someip_inet_*/AF_INET/in_addr via net_impl.h
#include "platform/net.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <variant>

namespace someip::sd {

// NOLINTBEGIN(misc-include-cleaner) - someip_hton*/someip_ntoh*/someip_inet_*/AF_INET/in_addr/
// INET_ADDRSTRLEN are macros and types from platform/byteorder.h and platform/net.h that
// misc-include-cleaner cannot trace through the abstraction layer.

/**
 * @brief Service Discovery message serialization
 * @satisfies feat_req_someipsd_300
 * @satisfies feat_req_someipsd_301
 */

// SdEntry serialization/deserialization
/** @implements REQ_ARCH_001, REQ_SD_001, REQ_SD_002, REQ_SD_003, REQ_SD_004, REQ_SD_005, REQ_SD_006, REQ_SD_007, REQ_SD_010, REQ_SD_011, REQ_SD_012, REQ_SD_013, REQ_SD_014, REQ_SD_020, REQ_SD_021, REQ_SD_022, REQ_SD_023, REQ_SD_024, REQ_SD_025, REQ_SD_026, REQ_SD_030, REQ_SD_031, REQ_SD_032, REQ_SD_033, REQ_SD_034, REQ_SD_035 */
platform::ByteBuffer SdEntry::serialize() const {
    platform::ByteBuffer data;
    data.reserve(16);  // SD entry is exactly 16 bytes per SOME/IP-SD spec

    // Byte 0: Type
    data.push_back(static_cast<uint8_t>(type_));

    // Byte 1: Index 1st options run
    data.push_back(index1_);

    // Byte 2: Index 2nd options run
    data.push_back(index2_);

    // Byte 3: #Opt1 (upper 4 bits) | #Opt2 (lower 4 bits)
    data.push_back(static_cast<uint8_t>((static_cast<uint32_t>(num_opts1_) << 4U) |
                                        (static_cast<uint32_t>(num_opts2_) & 0x0FU)));

    // Bytes 4-5: Service ID — derived classes override
    data.push_back(0);
    data.push_back(0);

    // Bytes 6-7: Instance ID — derived classes override
    data.push_back(0);
    data.push_back(0);

    // Byte 8: Major Version — derived classes override
    data.push_back(0);

    // Bytes 9-11: TTL (24-bit)
    data.push_back(static_cast<uint8_t>((ttl_ >> 16U) & 0xFFU));
    data.push_back(static_cast<uint8_t>((ttl_ >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(ttl_ & 0xFFU));

    // Bytes 12-15: Minor Version or EventGroup fields — derived classes override
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    return data;
}

/** @implements REQ_SD_001_E01, REQ_SD_001_E02, REQ_SD_010_E01, REQ_SD_010_E02, REQ_SD_020_E01, REQ_SD_020_E02, REQ_SD_021_E01, REQ_SD_022_E01 */
bool SdEntry::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (offset + 16 > data.size()) {
        return false;
    }

    type_ = static_cast<EntryType>(data[offset++]);   // byte 0
    index1_ = data[offset++];                          // byte 1
    index2_ = data[offset++];                          // byte 2
    const uint8_t opts_byte = data[offset++];          // byte 3
    num_opts1_ = static_cast<uint8_t>((static_cast<uint32_t>(opts_byte) >> 4U) & 0x0FU);
    num_opts2_ = static_cast<uint8_t>(static_cast<uint32_t>(opts_byte) & 0x0FU);

    // Bytes 4-15 handled by derived classes
    return true;
}

// ServiceEntry implementation
/** @implements REQ_SD_040, REQ_SD_041, REQ_SD_042, REQ_SD_043, REQ_SD_044, REQ_SD_045, REQ_SD_046, REQ_SD_050, REQ_SD_051, REQ_SD_052, REQ_SD_053, REQ_SD_054, REQ_SD_055, REQ_SD_056 */
platform::ByteBuffer ServiceEntry::serialize() const {
    platform::ByteBuffer data = SdEntry::serialize();

    // Bytes 4-5: Service ID
    data[4] = static_cast<uint8_t>((static_cast<uint32_t>(service_id_) >> 8U) & 0xFFU);
    data[5] = static_cast<uint8_t>(service_id_ & 0xFFU);

    // Bytes 6-7: Instance ID
    data[6] = static_cast<uint8_t>((static_cast<uint32_t>(instance_id_) >> 8U) & 0xFFU);
    data[7] = static_cast<uint8_t>(instance_id_ & 0xFFU);

    // Byte 8: Major Version
    data[8] = major_version_;

    // Bytes 12-15: Minor Version (32-bit per SOME/IP-SD spec)
    data[12] = static_cast<uint8_t>((minor_version_ >> 24U) & 0xFFU);
    data[13] = static_cast<uint8_t>((minor_version_ >> 16U) & 0xFFU);
    data[14] = static_cast<uint8_t>((minor_version_ >> 8U) & 0xFFU);
    data[15] = static_cast<uint8_t>(minor_version_ & 0xFFU);

    return data;
}

/** @implements REQ_SD_040_E01, REQ_SD_041_E01, REQ_SD_044_E01, REQ_SD_050_E01, REQ_SD_052_E01 */
bool ServiceEntry::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (!SdEntry::deserialize(data, offset)) {
        return false;
    }

    if (offset + 12 > data.size()) {
        return false;
    }

    // SdEntry::deserialize consumed bytes 0-4 (type, idx1, idx2, opts, skip).
    // We're now at byte 5 within the 16-byte entry.
    service_id_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset]) << 8U) |
                                        static_cast<uint32_t>(data[offset + 1]));
    instance_id_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset + 2]) << 8U) |
                                         static_cast<uint32_t>(data[offset + 3]));
    major_version_ = data[offset + 4];
    ttl_ = (static_cast<uint32_t>(data[offset + 5]) << 16U) | (static_cast<uint32_t>(data[offset + 6]) << 8U) |
           static_cast<uint32_t>(data[offset + 7]);
    minor_version_ = (static_cast<uint32_t>(data[offset + 8]) << 24U) |
                     (static_cast<uint32_t>(data[offset + 9]) << 16U) |
                     (static_cast<uint32_t>(data[offset + 10]) << 8U) |
                     static_cast<uint32_t>(data[offset + 11]);

    offset += 12;
    return true;
}

// EventGroupEntry implementation
/** @implements REQ_SD_060, REQ_SD_061, REQ_SD_062, REQ_SD_063, REQ_SD_064, REQ_SD_065, REQ_SD_066, REQ_SD_067, REQ_SD_068, REQ_SD_069, REQ_SD_070, REQ_SD_071, REQ_SD_072, REQ_SD_073, REQ_SD_074, REQ_SD_075, REQ_SD_076, REQ_SD_077 */
platform::ByteBuffer EventGroupEntry::serialize() const {
    platform::ByteBuffer data = SdEntry::serialize();

    // Bytes 4-5: Service ID
    data[4] = static_cast<uint8_t>((static_cast<uint32_t>(service_id_) >> 8U) & 0xFFU);
    data[5] = static_cast<uint8_t>(service_id_ & 0xFFU);

    // Bytes 6-7: Instance ID
    data[6] = static_cast<uint8_t>((static_cast<uint32_t>(instance_id_) >> 8U) & 0xFFU);
    data[7] = static_cast<uint8_t>(instance_id_ & 0xFFU);

    // Byte 8: Major Version
    data[8] = major_version_;

    // Bytes 12-13: Reserved + Counter (left as zero from base)
    // Bytes 14-15: EventGroup ID
    data[14] = static_cast<uint8_t>((static_cast<uint32_t>(eventgroup_id_) >> 8U) & 0xFFU);
    data[15] = static_cast<uint8_t>(eventgroup_id_ & 0xFFU);

    return data;
}

/** @implements REQ_SD_060_E01, REQ_SD_060_E02, REQ_SD_061_E01, REQ_SD_062_E01, REQ_SD_064_E01, REQ_SD_070_E01, REQ_SD_075_E01 */
bool EventGroupEntry::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (!SdEntry::deserialize(data, offset)) {
        return false;
    }

    if (offset + 12 > data.size()) {
        return false;
    }

    service_id_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset]) << 8U) |
                                        static_cast<uint32_t>(data[offset + 1]));
    instance_id_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset + 2]) << 8U) |
                                         static_cast<uint32_t>(data[offset + 3]));
    major_version_ = data[offset + 4];
    ttl_ = (static_cast<uint32_t>(data[offset + 5]) << 16U) | (static_cast<uint32_t>(data[offset + 6]) << 8U) |
           static_cast<uint32_t>(data[offset + 7]);
    eventgroup_id_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset + 10]) << 8U) |
                                           static_cast<uint32_t>(data[offset + 11]));

    offset += 12;
    return true;
}

// SdOption serialization/deserialization
platform::ByteBuffer SdOption::serialize() const {
    platform::ByteBuffer data;

    // Length (2 bytes)
    data.push_back(static_cast<uint8_t>((static_cast<uint32_t>(length_) >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(length_ & 0xFFU));

    // Type (1 byte)
    data.push_back(static_cast<uint8_t>(type_));

    // Reserved (1 byte)
    data.push_back(0);

    return data;
}

bool SdOption::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (offset + 4 > data.size()) {
        return false;
    }

    length_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset]) << 8U) |
                                    static_cast<uint32_t>(data[offset + 1]));
    offset += 2;

    type_ = static_cast<OptionType>(data[offset++]);
    offset++;  // Skip reserved byte

    return true;
}

// IPv4EndpointOption implementation
/** @implements REQ_SD_120, REQ_SD_122, REQ_SD_123 */
platform::ByteBuffer IPv4EndpointOption::serialize() const {
    platform::ByteBuffer data = SdOption::serialize();

    // IPv4 Address (4 bytes, network byte order on the wire)
    // ipv4_address_ stores addr.s_addr (NBO in memory); convert to host order
    // so that MSB-first shifts produce correct NBO wire bytes.
    uint32_t const host_addr = someip_ntohl(ipv4_address_);
    data.push_back(static_cast<uint8_t>((host_addr >> 24U) & 0xFFU));
    data.push_back(static_cast<uint8_t>((host_addr >> 16U) & 0xFFU));
    data.push_back(static_cast<uint8_t>((host_addr >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(host_addr & 0xFFU));

    // Reserved (1 byte)
    data.push_back(0);

    // Protocol (1 byte)
    data.push_back(protocol_);

    // Port (2 bytes) — shifts produce big-endian (NBO) wire bytes directly
    data.push_back(static_cast<uint8_t>((static_cast<uint32_t>(port_) >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(port_ & 0xFFU));

    // Length covers everything except Length(2) and Type(1) fields:
    // Reserved(1) + IPv4(4) + Reserved(1) + Proto(1) + Port(2) = 9
    uint16_t const length = 9;
    data[0] = static_cast<uint8_t>((static_cast<uint32_t>(length) >> 8U) & 0xFFU);
    data[1] = static_cast<uint8_t>(length & 0xFFU);

    return data;
}

/** @implements REQ_SD_064_E01 */
bool IPv4EndpointOption::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (!SdOption::deserialize(data, offset)) {
        return false;
    }

    if (length_ != 9) {
        return false;
    }

    constexpr size_t payload_bytes = 8;
    if (offset + payload_bytes > data.size()) {
        return false;
    }

    // IPv4 Address (4 wire bytes in NBO → host-order uint32 → s_addr via htonl)
    uint32_t const host_addr =
        (static_cast<uint32_t>(data[offset]) << 24U) |
        (static_cast<uint32_t>(data[offset + 1]) << 16U) |
        (static_cast<uint32_t>(data[offset + 2]) << 8U) |
        static_cast<uint32_t>(data[offset + 3]);
    ipv4_address_ = someip_htonl(host_addr);
    offset += 4;

    // Validate IP address (REQ_SD_064_E01)
    if (ipv4_address_ == 0 || ipv4_address_ == 0xFFFFFFFFU) {
        std::cout << "Warning: Invalid IP address in endpoint option: "
                  << ((ipv4_address_ >> 24U) & 0xFFU) << "."
                  << ((ipv4_address_ >> 16U) & 0xFFU) << "."
                  << ((ipv4_address_ >> 8U) & 0xFFU) << "."
                  << (ipv4_address_ & 0xFFU) << '\n';
        // Continue processing despite invalid address
    }

    // Skip reserved byte
    offset++;

    // Protocol (1 byte)
    protocol_ = data[offset++];

    // Port (2 bytes) — shifts reconstruct host-order value directly from NBO wire bytes
    port_ = static_cast<uint16_t>((static_cast<uint32_t>(data[offset]) << 8U) |
                                  static_cast<uint32_t>(data[offset + 1]));
    offset += 2;

    return true;
}

void IPv4EndpointOption::set_ipv4_address_from_string(const platform::String<>& ip_address) {
    struct in_addr addr{};
    if (someip_inet_pton(AF_INET, ip_address.c_str(), &addr) == 1) {
        ipv4_address_ = addr.s_addr;
    } else {
        ipv4_address_ = 0;
    }
}

platform::String<> IPv4EndpointOption::get_ipv4_address_string() const {
    std::array<char, INET_ADDRSTRLEN> buffer{};
    struct in_addr addr{};
    addr.s_addr = ipv4_address_;  // Already in network byte order
    someip_inet_ntop(AF_INET, &addr, buffer.data(), buffer.size());
    return platform::String<>(buffer.data());
}

// IPv4MulticastOption implementation
/** @implements REQ_SD_132, REQ_SD_160 */
platform::ByteBuffer IPv4MulticastOption::serialize() const {
    platform::ByteBuffer data = SdOption::serialize();

    // IPv4 Address (4 bytes, NBO on wire)
    uint32_t const host_addr = someip_ntohl(ipv4_address_);
    data.push_back(static_cast<uint8_t>((host_addr >> 24U) & 0xFFU));
    data.push_back(static_cast<uint8_t>((host_addr >> 16U) & 0xFFU));
    data.push_back(static_cast<uint8_t>((host_addr >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(host_addr & 0xFFU));

    // Reserved (1 byte)
    data.push_back(0);

    // Protocol (1 byte)
    data.push_back(protocol_);

    // Port (2 bytes) — shifts produce big-endian (NBO) wire bytes directly
    data.push_back(static_cast<uint8_t>((static_cast<uint32_t>(port_) >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(port_ & 0xFFU));

    // Length covers everything except Length(2) and Type(1):
    // Reserved(1) + IPv4(4) + Reserved(1) + Proto(1) + Port(2) = 9
    uint16_t const length = 9;
    data[0] = static_cast<uint8_t>((static_cast<uint32_t>(length) >> 8U) & 0xFFU);
    data[1] = static_cast<uint8_t>(length & 0xFFU);

    return data;
}

/** @implements REQ_SD_064_E01 */
bool IPv4MulticastOption::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (!SdOption::deserialize(data, offset)) {
        return false;
    }

    if (length_ != 9) {
        return false;
    }

    constexpr size_t payload_bytes = 8;
    if (offset + payload_bytes > data.size()) {
        return false;
    }

    uint32_t const host_addr =
        (static_cast<uint32_t>(data[offset]) << 24U) |
        (static_cast<uint32_t>(data[offset + 1]) << 16U) |
        (static_cast<uint32_t>(data[offset + 2]) << 8U) |
        static_cast<uint32_t>(data[offset + 3]);
    ipv4_address_ = someip_htonl(host_addr);
    offset += 4;

    // Validate IP address (REQ_SD_064_E01)
    if (ipv4_address_ == 0 || ipv4_address_ == 0xFFFFFFFFU) {
        std::cout << "Warning: Invalid IP address in multicast option: "
                  << ((host_addr >> 24U) & 0xFFU) << "."
                  << ((host_addr >> 16U) & 0xFFU) << "."
                  << ((host_addr >> 8U) & 0xFFU) << "."
                  << (host_addr & 0xFFU) << '\n';
    }

    // Skip reserved byte
    offset++;

    // Protocol (1 byte)
    protocol_ = data[offset++];

    // Port (2 bytes) — shifts reconstruct host-order value from NBO wire bytes
    port_ = static_cast<uint16_t>(
        (static_cast<uint32_t>(data[offset]) << 8U) |
        static_cast<uint32_t>(data[offset + 1]));
    offset += 2;

    return true;
}

// ConfigurationOption implementation
/** @implements REQ_SD_236, REQ_SD_243 */
platform::ByteBuffer ConfigurationOption::serialize() const {
    platform::ByteBuffer data = SdOption::serialize();

    // Configuration string
    const auto* str_begin = reinterpret_cast<const uint8_t*>(config_string_.data());
    data.insert(data.end(), str_begin, str_begin + config_string_.size());

    // Length covers everything after Length(2) and Type(1) fields:
    // Reserved(1) + config_string = 1 + config_string_.size()
    const auto length = static_cast<uint16_t>(1 + config_string_.size());
    data[0] = static_cast<uint8_t>((static_cast<uint32_t>(length) >> 8U) & 0xFFU);
    data[1] = static_cast<uint8_t>(length & 0xFFU);

    return data;
}

/** @implements REQ_SD_236, REQ_SD_243 */
bool ConfigurationOption::deserialize(const platform::ByteBuffer& data, size_t& offset) {
    if (!SdOption::deserialize(data, offset)) {
        return false;
    }

    // length_ includes the Reserved byte (already consumed by SdOption::deserialize).
    // The actual config string length is length_ - 1.
    if (length_ < 1) {
        return false;
    }
    const uint16_t config_len = length_ - 1;

    if (offset + config_len > data.size()) {
        return false;
    }
    if (config_len > config_string_.max_size()) {
        return false;
    }

    // Extract configuration string
    const auto* str_start = reinterpret_cast<const char*>(data.data() + offset);
    config_string_.assign(str_start, str_start + static_cast<std::ptrdiff_t>(config_len));
    offset += config_len;

    return true;
}

// SdMessage implementation
void SdMessage::add_entry(SdEntryStorage entry) {
    entries_.emplace_back(std::move(entry));
}

void SdMessage::add_option(SdOptionStorage option) {
    options_.emplace_back(std::move(option));
}

/** @implements REQ_SD_200A, REQ_SD_200B, REQ_SD_200C, REQ_SD_201, REQ_SD_202, REQ_SD_261, REQ_SD_282, REQ_SD_291, REQ_SD_301, REQ_SD_302, REQ_SD_303, REQ_SD_320 */
platform::ByteBuffer SdMessage::serialize() const {
    platform::ByteBuffer data;

    // Flags (1 byte) - ensure reserved bits 5-0 are zero (REQ_SD_013)
    auto const flags_to_send = static_cast<uint8_t>(static_cast<uint32_t>(flags_) & 0xC0U);
    data.push_back(flags_to_send);

    // Reserved (3 bytes)
    data.push_back(static_cast<uint8_t>((reserved_ >> 16U) & 0xFFU));
    data.push_back(static_cast<uint8_t>((reserved_ >> 8U) & 0xFFU));
    data.push_back(static_cast<uint8_t>(reserved_ & 0xFFU));

    // Length of Entries Array (4 bytes) - placeholder
    const size_t entries_len_offset = data.size();
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    // Entries Array
    const size_t entries_start = data.size();
    for (const auto& entry_var : entries_) {
        auto entry_data = std::visit([](const auto& e) { return e.serialize(); }, entry_var);
        if (entry_data.empty()) {
            return {};
        }
        data.insert(data.end(), entry_data.begin(), entry_data.end());
    }
    auto const entries_length = static_cast<uint32_t>(data.size() - entries_start);

    // Back-fill Length of Entries Array
    data[entries_len_offset]     = static_cast<uint8_t>((entries_length >> 24U) & 0xFFU);
    data[entries_len_offset + 1] = static_cast<uint8_t>((entries_length >> 16U) & 0xFFU);
    data[entries_len_offset + 2] = static_cast<uint8_t>((entries_length >> 8U) & 0xFFU);
    data[entries_len_offset + 3] = static_cast<uint8_t>(entries_length & 0xFFU);

    // Length of Options Array (4 bytes) - placeholder
    const size_t options_len_offset = data.size();
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    // Options Array
    const size_t options_start = data.size();
    for (const auto& option_var : options_) {
        auto option_data = std::visit([](const auto& o) { return o.serialize(); }, option_var);
        if (option_data.empty()) {
            return {};
        }
        data.insert(data.end(), option_data.begin(), option_data.end());
    }
    auto const options_length = static_cast<uint32_t>(data.size() - options_start);

    // Back-fill Length of Options Array
    data[options_len_offset]     = static_cast<uint8_t>((options_length >> 24U) & 0xFFU);
    data[options_len_offset + 1] = static_cast<uint8_t>((options_length >> 16U) & 0xFFU);
    data[options_len_offset + 2] = static_cast<uint8_t>((options_length >> 8U) & 0xFFU);
    data[options_len_offset + 3] = static_cast<uint8_t>(options_length & 0xFFU);

    return data;
}

/** @implements REQ_SD_030_E01, REQ_SD_200A, REQ_SD_200B, REQ_SD_200C, REQ_SD_201, REQ_SD_202, REQ_SD_261, REQ_SD_282, REQ_SD_291, REQ_SD_301, REQ_SD_302, REQ_SD_303, REQ_SD_320 */
bool SdMessage::deserialize(const platform::ByteBuffer& data) {
    if (data.size() < 12) {
        return false;
    }

    size_t offset = 0;

    // Flags (1 byte) + Reserved (3 bytes)
    flags_ = data[offset++];
    reserved_ = (static_cast<uint32_t>(data[offset]) << 16U) | (static_cast<uint32_t>(data[offset + 1]) << 8U) |
              static_cast<uint32_t>(data[offset + 2]);
    offset += 3;

    // Length of Entries Array (4 bytes)
    const uint32_t entries_length = (static_cast<uint32_t>(data[offset]) << 24U) |
                                    (static_cast<uint32_t>(data[offset + 1]) << 16U) |
                                    (static_cast<uint32_t>(data[offset + 2]) << 8U) |
                                    static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    if (offset + entries_length > data.size()) {
        return false;
    }

    if (entries_length % 16 != 0) {
        return false;
    }

    // Parse entries (each entry is exactly 16 bytes)
    size_t const entries_end = offset + entries_length;
    while (offset + 16 <= entries_end) {
        const uint8_t raw_entry_type = data[offset];

        if (raw_entry_type == 0x00 || raw_entry_type == 0x01) {
            ServiceEntry entry;
            if (!entry.deserialize(data, offset)) {
                return false;
            }
            entries_.emplace_back(std::move(entry));
        } else if (raw_entry_type == 0x06 || raw_entry_type == 0x07) {
            EventGroupEntry entry;
            if (!entry.deserialize(data, offset)) {
                return false;
            }
            entries_.emplace_back(std::move(entry));
        } else {
            return false;
        }
    }

    // Length of Options Array (4 bytes)
    if (offset + 4 > data.size()) {
        return true;  // no options section present
    }
    const uint32_t options_length = (static_cast<uint32_t>(data[offset]) << 24U) |
                                    (static_cast<uint32_t>(data[offset + 1]) << 16U) |
                                    (static_cast<uint32_t>(data[offset + 2]) << 8U) |
                                    static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    if (offset + options_length > data.size()) {
        return false;
    }

    // Parse options
    size_t const options_end = offset + options_length;
    while (offset < options_end) {
        if (offset + 4 > data.size()) {
            return false;
        }

        // Options start with length(2) + type(1) + reserved(1).
        // Peek at the type byte (offset + 2) to determine the option kind.
        const uint8_t type_byte = data[offset + 2];
        auto const option_type = static_cast<OptionType>(type_byte);

        if (option_type == OptionType::CONFIGURATION) {
            ConfigurationOption option;
            if (!option.deserialize(data, offset)) {
                return false;
            }
            options_.emplace_back(std::move(option));
        } else if (option_type == OptionType::IPV4_ENDPOINT) {
            IPv4EndpointOption option;
            if (!option.deserialize(data, offset)) {
                return false;
            }
            options_.emplace_back(std::move(option));
        } else if (option_type == OptionType::IPV4_MULTICAST) {
            IPv4MulticastOption option;
            if (!option.deserialize(data, offset)) {
                return false;
            }
            options_.emplace_back(std::move(option));
        } else {
            // Total option size = Length(2) + Type(1) + length_value
            // where length_value includes Reserved(1) + option-specific data
            auto const option_len = static_cast<uint16_t>(
                (static_cast<uint32_t>(data[offset]) << 8U) | static_cast<uint32_t>(data[offset + 1]));
            if (offset + 3 + option_len > options_end) {
                return false;
            }
            offset += 3 + option_len;
            continue;
        }
    }

    return true;
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::sd
