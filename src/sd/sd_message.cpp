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
#include "serialization/serializer.h"
#include <algorithm>
#include <arpa/inet.h>
#include <iostream>

namespace someip {
namespace sd {

/**
 * @brief Service Discovery message serialization
 * @implements REQ_ARCH_001
 * @implements REQ_SD_001, REQ_SD_002, REQ_SD_003, REQ_SD_004, REQ_SD_005, REQ_SD_006, REQ_SD_007
 * @implements REQ_SD_010, REQ_SD_011, REQ_SD_012, REQ_SD_013, REQ_SD_014
 * @implements REQ_SD_020, REQ_SD_021, REQ_SD_022, REQ_SD_023, REQ_SD_024, REQ_SD_025, REQ_SD_026
 * @implements REQ_SD_030, REQ_SD_031, REQ_SD_032, REQ_SD_033, REQ_SD_034, REQ_SD_035
 * @implements REQ_SD_040, REQ_SD_041, REQ_SD_042, REQ_SD_043, REQ_SD_044, REQ_SD_045, REQ_SD_046
 * @implements REQ_SD_050, REQ_SD_051, REQ_SD_052, REQ_SD_053, REQ_SD_054, REQ_SD_055, REQ_SD_056
 * @implements REQ_SD_060, REQ_SD_061, REQ_SD_062, REQ_SD_063, REQ_SD_064, REQ_SD_065
 * @implements REQ_SD_066, REQ_SD_067, REQ_SD_068, REQ_SD_069
 * @implements REQ_SD_070, REQ_SD_071, REQ_SD_072, REQ_SD_073, REQ_SD_074, REQ_SD_075, REQ_SD_076, REQ_SD_077
 * @implements REQ_SD_001_E01, REQ_SD_010_E01
 * @implements REQ_SD_020_E01, REQ_SD_020_E02, REQ_SD_021_E01, REQ_SD_022_E01
 * @implements REQ_SD_040_E01, REQ_SD_041_E01
 * @implements REQ_SD_050_E01, REQ_SD_052_E01
 * @implements REQ_SD_060_E01, REQ_SD_061_E01, REQ_SD_062_E01, REQ_SD_064_E01, REQ_SD_075_E01
 * @satisfies feat_req_someipsd_300
 * @satisfies feat_req_someipsd_301
 */

// SdEntry serialization/deserialization
std::vector<uint8_t> SdEntry::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(16);  // SD entry is 16 bytes

    // Type (1 byte)
    data.push_back(static_cast<uint8_t>(type_));

    // Index 1 (1 byte)
    data.push_back(index1_);

    // Index 2 (1 byte)
    data.push_back(index2_);

    // Number of Options 1 (1 byte) - derived classes will handle
    data.push_back(0);

    // Number of Options 2 (1 byte) - derived classes will handle
    data.push_back(0);

    // Service ID (2 bytes) - derived classes will handle
    data.push_back(0);
    data.push_back(0);

    // Instance ID (2 bytes) - derived classes will handle
    data.push_back(0);
    data.push_back(0);

    // Major Version (1 byte) - derived classes will handle
    data.push_back(0);

    // TTL (4 bytes)
    data.push_back((ttl_ >> 24) & 0xFF);
    data.push_back((ttl_ >> 16) & 0xFF);
    data.push_back((ttl_ >> 8) & 0xFF);
    data.push_back(ttl_ & 0xFF);

    return data;
}

bool SdEntry::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset + 16 > data.size()) {
        return false;
    }

    type_ = static_cast<EntryType>(data[offset++]);
    index1_ = data[offset++];
    index2_ = data[offset++];
    offset += 2;  // Skip number of options (handled by SdMessage)

    // Service ID, Instance ID, Major Version, TTL will be handled by derived classes
    return true;
}

// ServiceEntry implementation
std::vector<uint8_t> ServiceEntry::serialize() const {
    std::vector<uint8_t> data = SdEntry::serialize();

    // Override the service ID field (bytes 4-5)
    data[4] = (service_id_ >> 8) & 0xFF;
    data[5] = service_id_ & 0xFF;

    // Override the instance ID field (bytes 6-7)
    data[6] = (instance_id_ >> 8) & 0xFF;
    data[7] = instance_id_ & 0xFF;

    // Override the major version field (byte 8)
    data[8] = major_version_;

    return data;
}

bool ServiceEntry::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (!SdEntry::deserialize(data, offset)) {
        return false;
    }

    if (offset + 9 > data.size()) {
        return false;
    }

    service_id_ = (data[offset] << 8) | data[offset + 1];
    instance_id_ = (data[offset + 2] << 8) | data[offset + 3];
    major_version_ = data[offset + 4];
    ttl_ = (data[offset + 5] << 24) | (data[offset + 6] << 16) |
           (data[offset + 7] << 8) | data[offset + 8];

    offset += 9;
    return true;
}

// EventGroupEntry implementation
std::vector<uint8_t> EventGroupEntry::serialize() const {
    std::vector<uint8_t> data = SdEntry::serialize();

    // Override the service ID field (bytes 4-5)
    data[4] = (service_id_ >> 8) & 0xFF;
    data[5] = service_id_ & 0xFF;

    // Override the instance ID field (bytes 6-7)
    data[6] = (instance_id_ >> 8) & 0xFF;
    data[7] = instance_id_ & 0xFF;

    // Override the major version field (byte 8)
    data[8] = major_version_;

    // Event group ID (bytes 9-10)
    data.push_back((eventgroup_id_ >> 8) & 0xFF);
    data.push_back(eventgroup_id_ & 0xFF);

    return data;
}

bool EventGroupEntry::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (!SdEntry::deserialize(data, offset)) {
        return false;
    }

    if (offset + 11 > data.size()) {
        return false;
    }

    service_id_ = (data[offset] << 8) | data[offset + 1];
    instance_id_ = (data[offset + 2] << 8) | data[offset + 3];
    major_version_ = data[offset + 4];
    ttl_ = (data[offset + 5] << 24) | (data[offset + 6] << 16) |
           (data[offset + 7] << 8) | data[offset + 8];
    eventgroup_id_ = (data[offset + 9] << 8) | data[offset + 10];

    offset += 11;
    return true;
}

// SdOption serialization/deserialization
std::vector<uint8_t> SdOption::serialize() const {
    std::vector<uint8_t> data;

    // Length (2 bytes)
    data.push_back((length_ >> 8) & 0xFF);
    data.push_back(length_ & 0xFF);

    // Type (1 byte)
    data.push_back(static_cast<uint8_t>(type_));

    // Reserved (1 byte)
    data.push_back(0);

    return data;
}

bool SdOption::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset + 4 > data.size()) {
        return false;
    }

    length_ = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    type_ = static_cast<OptionType>(data[offset++]);
    offset++;  // Skip reserved byte

    return true;
}

// IPv4EndpointOption implementation
std::vector<uint8_t> IPv4EndpointOption::serialize() const {
    std::vector<uint8_t> data = SdOption::serialize();

    // IPv4 Address (4 bytes, network byte order)
    // ipv4_address_ is already in network byte order
    data.push_back((ipv4_address_ >> 24) & 0xFF);
    data.push_back((ipv4_address_ >> 16) & 0xFF);
    data.push_back((ipv4_address_ >> 8) & 0xFF);
    data.push_back(ipv4_address_ & 0xFF);

    // Reserved (1 byte)
    data.push_back(0);

    // Protocol (1 byte)
    data.push_back(protocol_);

    // Port (2 bytes, network byte order)
    uint16_t network_port = htons(port_);
    data.push_back((network_port >> 8) & 0xFF);
    data.push_back(network_port & 0xFF);

    // Update length (8 bytes of data after the option header)
    uint16_t length = 8;
    data[0] = (length >> 8) & 0xFF;
    data[1] = length & 0xFF;

    return data;
}

bool IPv4EndpointOption::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (!SdOption::deserialize(data, offset)) {
        return false;
    }

    if (offset + length_ > data.size()) {
        return false;
    }

    // IPv4 Address (4 bytes, network byte order)
    ipv4_address_ = (data[offset] << 24) | (data[offset + 1] << 16) |
                   (data[offset + 2] << 8) | data[offset + 3];
    offset += 4;

    // Validate IP address (REQ_SD_064_E01)
    if (ipv4_address_ == 0 || ipv4_address_ == 0xFFFFFFFF) {
        std::cout << "Warning: Invalid IP address in endpoint option: "
                  << ((ipv4_address_ >> 24) & 0xFF) << "."
                  << ((ipv4_address_ >> 16) & 0xFF) << "."
                  << ((ipv4_address_ >> 8) & 0xFF) << "."
                  << (ipv4_address_ & 0xFF) << std::endl;
        // Continue processing despite invalid address
    }

    // Skip reserved byte
    offset++;

    // Protocol (1 byte)
    protocol_ = data[offset++];

    // Port (2 bytes, network byte order)
    uint16_t network_port = (data[offset] << 8) | data[offset + 1];
    port_ = ntohs(network_port);
    offset += 2;

    return true;
}

void IPv4EndpointOption::set_ipv4_address_from_string(const std::string& ip_address) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_address.c_str(), &addr) == 1) {
        // inet_pton gives us network byte order, store as-is
        ipv4_address_ = addr.s_addr;
    } else {
        ipv4_address_ = 0;
    }
}

std::string IPv4EndpointOption::get_ipv4_address_string() const {
    char buffer[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = ipv4_address_;  // Already in network byte order
    inet_ntop(AF_INET, &addr, buffer, sizeof(buffer));
    return buffer;
}

// IPv4MulticastOption implementation
std::vector<uint8_t> IPv4MulticastOption::serialize() const {
    std::vector<uint8_t> data = SdOption::serialize();

    // IPv4 Address (4 bytes)
    data.push_back((ipv4_address_ >> 24) & 0xFF);
    data.push_back((ipv4_address_ >> 16) & 0xFF);
    data.push_back((ipv4_address_ >> 8) & 0xFF);
    data.push_back(ipv4_address_ & 0xFF);

    // Reserved (1 byte)
    data.push_back(0);

    // Port (2 bytes)
    data.push_back((port_ >> 8) & 0xFF);
    data.push_back(port_ & 0xFF);

    // Update length (7 bytes: 4 address + 1 reserved + 2 port)
    uint16_t length = 7;
    data[2] = (length >> 8) & 0xFF;
    data[3] = length & 0xFF;

    return data;
}

bool IPv4MulticastOption::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (!SdOption::deserialize(data, offset)) {
        return false;
    }

    if (offset + length_ > data.size()) {
        return false;
    }

    ipv4_address_ = (data[offset] << 24) | (data[offset + 1] << 16) |
                   (data[offset + 2] << 8) | data[offset + 3];
    offset += 5;  // Skip address + reserved

    // Validate IP address (REQ_SD_064_E01)
    if (ipv4_address_ == 0 || ipv4_address_ == 0xFFFFFFFF) {
        std::cout << "Warning: Invalid IP address in multicast option: "
                  << ((ipv4_address_ >> 24) & 0xFF) << "."
                  << ((ipv4_address_ >> 16) & 0xFF) << "."
                  << ((ipv4_address_ >> 8) & 0xFF) << "."
                  << (ipv4_address_ & 0xFF) << std::endl;
        // Continue processing despite invalid address
    }
    port_ = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    return true;
}

// ConfigurationOption implementation
std::vector<uint8_t> ConfigurationOption::serialize() const {
    std::vector<uint8_t> data;

    // Type (1 byte)
    data.push_back(static_cast<uint8_t>(OptionType::CONFIGURATION));

    // Reserved (1 byte)
    data.push_back(0);

    // Length (2 bytes) - will be filled later
    data.push_back(0);
    data.push_back(0);

    // Configuration string
    data.insert(data.end(), config_string_.begin(), config_string_.end());

    // Update length
    uint16_t length = static_cast<uint16_t>(config_string_.size());
    data[2] = (length >> 8) & 0xFF;
    data[3] = length & 0xFF;

    return data;
}

bool ConfigurationOption::deserialize(const std::vector<uint8_t>& data, size_t& offset) {
    if (!SdOption::deserialize(data, offset)) {
        return false;
    }

    if (offset + length_ > data.size()) {
        return false;
    }

    // Extract configuration string
    config_string_.assign(data.begin() + offset, data.begin() + offset + length_);
    offset += length_;

    return true;
}

// SdMessage implementation
void SdMessage::add_entry(std::unique_ptr<SdEntry> entry) {
    entries_.push_back(std::move(entry));
}

void SdMessage::add_option(std::unique_ptr<SdOption> option) {
    options_.push_back(std::move(option));
}

std::vector<uint8_t> SdMessage::serialize() const {
    std::vector<uint8_t> data;

    // SOME/IP SD Header (8 bytes)
    // Flags (1 byte) - ensure reserved bits 5-0 are zero (REQ_SD_013)
    uint8_t flags_to_send = flags_ & 0xC0;  // Keep only bits 7 and 6
    data.push_back(flags_to_send);

    // Reserved (3 bytes) - we use 4 bytes total for reserved_
    data.push_back((reserved_ >> 16) & 0xFF);
    data.push_back((reserved_ >> 8) & 0xFF);
    data.push_back(reserved_ & 0xFF);

    // Length (4 bytes) - placeholder, will be filled later
    size_t length_offset = data.size();
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    // Entries
    for (const auto& entry : entries_) {
        auto entry_data = entry->serialize();
        data.insert(data.end(), entry_data.begin(), entry_data.end());
    }

    // Options
    for (const auto& option : options_) {
        auto option_data = option->serialize();
        data.insert(data.end(), option_data.begin(), option_data.end());
    }

    // Update length (total length - 8 byte header)
    uint32_t total_length = data.size() - 8;
    data[length_offset] = (total_length >> 24) & 0xFF;
    data[length_offset + 1] = (total_length >> 16) & 0xFF;
    data[length_offset + 2] = (total_length >> 8) & 0xFF;
    data[length_offset + 3] = total_length & 0xFF;

    return data;
}

bool SdMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8) {
        return false;
    }

    size_t offset = 0;

    // SOME/IP SD Header
    flags_ = data[offset++];
    reserved_ = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;

    // Note: Reserved bits 5-0 in flags are ignored (REQ_SD_014)

    uint32_t length = (data[offset] << 24) | (data[offset + 1] << 16) |
                     (data[offset + 2] << 8) | data[offset + 3];
    offset += 4;

    if (offset + length > data.size()) {
        return false;
    }

    // Check if entries length is a multiple of entry size (16 bytes) (REQ_SD_020_E02)
    if (length % 16 != 0) {
        std::cout << "Warning: SD entries length " << length
                  << " is not a multiple of entry size (16 bytes)" << std::endl;
        // Continue processing but log warning
    }

    // Parse entries and options until we consume all data
    size_t max_iterations = 100; // Prevent infinite loops
    size_t iteration = 0;

    while (offset < 8 + length && offset < data.size() && iteration < max_iterations) {
        iteration++;

        uint8_t type_byte = data[offset];

        // Check if this is an entry (entries come first in SOME/IP SD)
        EntryType entry_type = static_cast<EntryType>(type_byte);
        uint8_t raw_entry_type = static_cast<uint8_t>(entry_type);

        if (raw_entry_type == 0x00 || raw_entry_type == 0x01 ||
            raw_entry_type == 0x06 || raw_entry_type == 0x07) {

            // This is an entry
            std::unique_ptr<SdEntry> entry;

            if (raw_entry_type == 0x00 || raw_entry_type == 0x01) {
                entry = std::make_unique<ServiceEntry>();
            } else if (raw_entry_type == 0x06 || raw_entry_type == 0x07) {
                entry = std::make_unique<EventGroupEntry>();
            }

            if (!entry || !entry->deserialize(data, offset)) {
                return false; // Failed to parse entry
            }

            entries_.push_back(std::move(entry));
        } else {
            // This should be an option
            OptionType option_type = static_cast<OptionType>(type_byte);
            std::unique_ptr<SdOption> option;

            if (option_type == OptionType::CONFIGURATION) {
                option = std::make_unique<ConfigurationOption>();
            } else if (option_type == OptionType::IPV4_ENDPOINT) {
                option = std::make_unique<IPv4EndpointOption>();
            } else if (option_type == OptionType::IPV4_MULTICAST) {
                option = std::make_unique<IPv4MulticastOption>();
            } else {
                // Unknown option type - skip with warning (REQ_SD_061_E01)
                std::cout << "Warning: Unknown SD option type 0x" << std::hex << (int)type_byte
                          << ", skipping option" << std::endl;
                if (offset + 4 > data.size()) {
                    return false;
                }
                uint16_t option_length = (data[offset] << 8) | data[offset + 1];
                if (offset + 4 + option_length > data.size()) {
                    return false;
                }
                offset += 4 + option_length;
                continue;
            }

            if (!option || !option->deserialize(data, offset)) {
                return false; // Failed to parse option
            }

            options_.push_back(std::move(option));
        }
    }

    // Check if we consumed all expected data
    if (offset != 8 + length) {
        return false; // Didn't consume all data or overran
    }

    return true;
}

} // namespace sd
} // namespace someip
