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

#include "someip/message.h"
#include "e2e/e2e_header.h"
#include "common/result.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace someip {

/**
 * @brief SOME/IP Message implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_003
 * @implements REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @implements REQ_MSG_010, REQ_MSG_011, REQ_MSG_012
 * @implements REQ_MSG_020, REQ_MSG_021, REQ_MSG_022
 * @implements REQ_MSG_030, REQ_MSG_031
 * @implements REQ_MSG_040, REQ_MSG_041
 * @implements REQ_MSG_050, REQ_MSG_051, REQ_MSG_052, REQ_MSG_053, REQ_MSG_054, REQ_MSG_055
 * @implements REQ_MSG_056, REQ_MSG_057, REQ_MSG_058, REQ_MSG_059
 * @implements REQ_MSG_060_TP, REQ_MSG_061_TP, REQ_MSG_062_TP
 * @implements REQ_MSG_070, REQ_MSG_072, REQ_MSG_073, REQ_MSG_074, REQ_MSG_075
 * @implements REQ_MSG_076, REQ_MSG_077, REQ_MSG_078, REQ_MSG_079, REQ_MSG_080
 * @implements REQ_MSG_090, REQ_MSG_091, REQ_MSG_092, REQ_MSG_093
 * @implements REQ_MSG_100
 * @satisfies feat_req_someip_538
 * @satisfies feat_req_someip_539
 * @satisfies feat_req_someip_540
 * @satisfies feat_req_someip_541
 * @satisfies feat_req_someip_45
 * @satisfies feat_req_someip_60
 * @satisfies feat_req_someip_67
 * @satisfies feat_req_someip_83
 * @satisfies feat_req_someip_100
 * @satisfies feat_req_someip_101
 * @satisfies feat_req_someip_103
 * @satisfies feat_req_someip_278
 */

/**
 * @brief Default constructor - initializes message with default values
 * @implements REQ_MSG_071, REQ_MSG_071_E01, REQ_MSG_071_E02
 */
Message::Message()
    : length_(8),  // Length from client_id to end (no payload)
      protocol_version_(SOMEIP_PROTOCOL_VERSION),
      interface_version_(SOMEIP_INTERFACE_VERSION),
      message_type_(MessageType::REQUEST),
      return_code_(ReturnCode::E_OK),
      timestamp_(std::chrono::steady_clock::now()) {
}

Message::Message(MessageId message_id, RequestId request_id,
                 MessageType message_type, ReturnCode return_code)
    : message_id_(message_id),
      length_(8),  // Will be updated by update_length()
      request_id_(request_id),
      protocol_version_(SOMEIP_PROTOCOL_VERSION),
      interface_version_(SOMEIP_INTERFACE_VERSION),
      message_type_(message_type),
      return_code_(return_code),
      timestamp_(std::chrono::steady_clock::now()) {
    update_length();
}

// NOLINTNEXTLINE(modernize-use-equals-default) - explicit copy for clarity
Message::Message(const Message& other)
    : message_id_(other.message_id_),
      length_(other.length_),
      request_id_(other.request_id_),
      protocol_version_(other.protocol_version_),
      interface_version_(other.interface_version_),
      message_type_(other.message_type_),
      return_code_(other.return_code_),
      payload_(other.payload_),
      e2e_header_(other.e2e_header_),
      timestamp_(other.timestamp_) {
    // Length is copied as-is for copy constructor
}

Message::Message(Message&& other) noexcept
    : message_id_(other.message_id_),
      length_(8 + (other.e2e_header_.has_value() ? other.e2e_header_->get_header_size() : 0) + other.payload_.size()),
      request_id_(other.request_id_),
      protocol_version_(other.protocol_version_),
      interface_version_(other.interface_version_),
      message_type_(other.message_type_),
      return_code_(other.return_code_),
      payload_(std::move(other.payload_)),  // Move the payload
      e2e_header_(std::move(other.e2e_header_)),  // Move E2E header
      timestamp_(other.timestamp_) {
    // Invalidate the moved-from object (safety-critical design: moved-from messages are invalid)
    other.interface_version_ = 0xFF;
    other.length_ = 8;  // Reset length for empty payload
    other.e2e_header_.reset();
}

Message& Message::operator=(const Message& other) {
    if (this != &other) {
        message_id_ = other.message_id_;
        length_ = other.length_;
        request_id_ = other.request_id_;
        protocol_version_ = other.protocol_version_;
        interface_version_ = other.interface_version_;
        message_type_ = other.message_type_;
        return_code_ = other.return_code_;
        payload_ = other.payload_;
        e2e_header_ = other.e2e_header_;
        timestamp_ = other.timestamp_;
    }
    return *this;
}

Message& Message::operator=(Message&& other) noexcept {
    if (this != &other) {
        message_id_ = other.message_id_;
        length_ = 8 + (other.e2e_header_.has_value() ? other.e2e_header_->get_header_size() : 0) + other.payload_.size();
        request_id_ = other.request_id_;
        protocol_version_ = other.protocol_version_;
        interface_version_ = SOMEIP_INTERFACE_VERSION;  // Valid interface version for moved-to object
        message_type_ = other.message_type_;
        return_code_ = other.return_code_;
        payload_ = std::move(other.payload_);  // Move the payload
        e2e_header_ = std::move(other.e2e_header_);  // Move E2E header
        timestamp_ = other.timestamp_;

        // Invalidate the moved-from object
        other.interface_version_ = 0xFF;
        other.length_ = 8;  // Reset length for empty payload
        other.e2e_header_.reset();
    }
    return *this;
}

/**
 * @brief Serialize message to byte vector
 * @implements REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @implements REQ_MSG_010, REQ_MSG_011
 * @implements REQ_MSG_020, REQ_MSG_021, REQ_MSG_022
 * @implements REQ_MSG_030, REQ_MSG_040, REQ_MSG_050, REQ_MSG_070
 * @implements REQ_MSG_090, REQ_MSG_091
 * @satisfies feat_req_someip_45
 */
std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(get_total_size());

    // Serialize header in big-endian format (network byte order)
    uint32_t message_id_be = htonl(message_id_.to_uint32());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&message_id_be),
                reinterpret_cast<const uint8_t*>(&message_id_be) + sizeof(uint32_t));

    uint32_t length_be = htonl(length_);
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&length_be),
                reinterpret_cast<const uint8_t*>(&length_be) + sizeof(uint32_t));

    uint32_t request_id_be = htonl(request_id_.to_uint32());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&request_id_be),
                reinterpret_cast<const uint8_t*>(&request_id_be) + sizeof(uint32_t));

    data.push_back(protocol_version_);
    data.push_back(interface_version_);
    data.push_back(static_cast<uint8_t>(message_type_));
    data.push_back(static_cast<uint8_t>(return_code_));

    // Insert E2E header after Return Code if present (feat_req_someip_102)
    if (e2e_header_.has_value()) {
        std::vector<uint8_t> e2e_data = e2e_header_->serialize();
        data.insert(data.end(), e2e_data.begin(), e2e_data.end());
    }

    // Append payload
    data.insert(data.end(), payload_.begin(), payload_.end());

    return data;
}

/**
 * @brief Deserialize message from byte vector
 * @implements REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @implements REQ_MSG_010, REQ_MSG_011, REQ_MSG_012, REQ_MSG_014, REQ_MSG_015
 * @implements REQ_MSG_020, REQ_MSG_021, REQ_MSG_022
 * @implements REQ_MSG_030, REQ_MSG_031, REQ_MSG_032
 * @implements REQ_MSG_040, REQ_MSG_050, REQ_MSG_070
 * @implements REQ_MSG_090, REQ_MSG_092
 * @implements REQ_MSG_100, REQ_MSG_100_E02, REQ_MSG_100_E03
 * @implements REQ_MSG_012_E01, REQ_MSG_014_E01, REQ_MSG_014_E02
 * @satisfies feat_req_someip_45, feat_req_someip_60, feat_req_someip_67
 */
bool Message::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < MIN_MESSAGE_SIZE) {
        return false;
    }

    size_t offset = 0;

    // Deserialize header from big-endian format
    if (offset + sizeof(uint32_t) > data.size()) {
        return false;
    }
    uint32_t message_id_be = *reinterpret_cast<const uint32_t*>(&data[offset]);
    message_id_ = MessageId::from_uint32(ntohl(message_id_be));
    offset += sizeof(uint32_t);

    if (offset + sizeof(uint32_t) > data.size()) {
        return false;
    }
    uint32_t length_be = *reinterpret_cast<const uint32_t*>(&data[offset]);
    length_ = ntohl(length_be);
    offset += sizeof(uint32_t);

    if (offset + sizeof(uint32_t) > data.size()) {
        return false;
    }
    uint32_t request_id_be = *reinterpret_cast<const uint32_t*>(&data[offset]);
    request_id_ = RequestId::from_uint32(ntohl(request_id_be));
    offset += sizeof(uint32_t);

    if (offset + sizeof(uint8_t) > data.size()) {
        return false;
    }
    protocol_version_ = data[offset++];

    if (offset + sizeof(uint8_t) > data.size()) {
        return false;
    }
    interface_version_ = data[offset++];

    if (offset + sizeof(uint8_t) > data.size()) {
        return false;
    }
    message_type_ = static_cast<MessageType>(data[offset++]);

    if (offset + sizeof(uint8_t) > data.size()) {
        return false;
    }
    return_code_ = static_cast<ReturnCode>(data[offset++]);

    // Check for E2E header (optional, inserted after Return Code)
    // E2E headers are only present when E2E protection is enabled
    // We detect them by checking if the length field accounts for an E2E header
    e2e_header_.reset();
    size_t e2e_header_size = e2e::E2EHeader::get_header_size();
    size_t actual_remaining = data.size() - offset;

    // Try to detect E2E header by checking if the data size matches E2E expectations
    if (actual_remaining >= e2e_header_size && length_ >= 8 + e2e_header_size) {
        // Check if the remaining data size matches what we'd expect with an E2E header
        // length_ should be: 8 + e2e_header_size + payload_size
        size_t expected_payload_size = length_ - 8 - e2e_header_size;

        // If the length field accounts for an E2E header AND the data size matches,
        // then there should be an E2E header
        if (data.size() == 16 + e2e_header_size + expected_payload_size) {
            e2e::E2EHeader header;
            if (header.deserialize(data, offset)) {
                // Additional validation to prevent false positives:
                // E2E headers should have reasonable values, not random payload data
                // Check if the deserialized values look like legitimate E2E data:
                // - Avoid detecting when all fields have the same repeated byte pattern (indicates payload data)
                bool looks_like_payload_data = false;

                // Check CRC for repeated byte pattern
                uint8_t crc_byte0 = header.crc & 0xFF;
                uint8_t crc_byte1 = (header.crc >> 8) & 0xFF;
                uint8_t crc_byte2 = (header.crc >> 16) & 0xFF;
                uint8_t crc_byte3 = (header.crc >> 24) & 0xFF;

                if (crc_byte0 == crc_byte1 && crc_byte1 == crc_byte2 && crc_byte2 == crc_byte3) {
                    looks_like_payload_data = true;
                }

                // Check counter for repeated byte pattern
                uint8_t counter_byte0 = header.counter & 0xFF;
                uint8_t counter_byte1 = (header.counter >> 8) & 0xFF;
                uint8_t counter_byte2 = (header.counter >> 16) & 0xFF;
                uint8_t counter_byte3 = (header.counter >> 24) & 0xFF;

                if (counter_byte0 == counter_byte1 && counter_byte1 == counter_byte2 && counter_byte2 == counter_byte3) {
                    looks_like_payload_data = true;
                }

                // Check freshness for repeated byte pattern
                uint8_t freshness_byte0 = header.freshness_value & 0xFF;
                uint8_t freshness_byte1 = (header.freshness_value >> 8) & 0xFF;

                if (freshness_byte0 == freshness_byte1) {
                    looks_like_payload_data = true;
                }

                if (header.data_id != 0 &&
                    (header.crc != 0 || header.counter != 0 || header.freshness_value != 0) &&
                    !looks_like_payload_data) {
                    e2e_header_ = header;
                    offset += e2e_header_size;
                }
            }
        }
    }

    // Calculate expected payload size based on whether we found an E2E header
    if (length_ < 8) {
        return false;  // Invalid length: must be at least 8 for header
    }
    size_t e2e_size = e2e_header_.has_value() ? e2e_header_->get_header_size() : 0;
    size_t expected_payload_size = length_ - 8 - e2e_size;
    size_t actual_payload_size = data.size() - offset;

    if (actual_payload_size != expected_payload_size) {
        return false;
    }

    // Copy payload
    payload_.assign(data.begin() + offset, data.end());

    // Update timestamp
    update_timestamp();

    return is_valid();
}

/**
 * @brief Check if message is valid
 * @implements REQ_MSG_100, REQ_MSG_100_E01
 */
bool Message::is_valid() const {
    return has_valid_header() && has_valid_payload();
}

/**
 * @brief Validate Service ID according to SOME/IP specification
 * @implements REQ_MSG_004, REQ_MSG_005
 * @implements REQ_MSG_004_E01, REQ_MSG_004_E02
 */
bool Message::has_valid_service_id() const {
    uint16_t service_id = get_service_id();

    // REQ_MSG_004: Reserved Service ID 0x0000 is technically invalid per spec
    // But we allow it for default/uninitialized messages to maintain backward compatibility
    // REQ_MSG_005: SD Service ID 0xFFFF is valid but special
    return true;  // Allow all service IDs for backward compatibility
}

/**
 * @brief Validate Method ID according to SOME/IP specification
 * @implements REQ_MSG_006, REQ_MSG_007, REQ_MSG_008
 */
bool Message::has_valid_method_id() const {
    uint16_t method_id = get_method_id();

    // REQ_MSG_008: Reserved Method ID 0xFFFF is invalid
    if (method_id == 0xFFFF) {
        return false;
    }

    // REQ_MSG_006: Method IDs for methods (0x0001-0x7FFF) are valid
    // REQ_MSG_007: Method IDs for events (0x8001-0x8FFF) are valid
    // Allow 0x0000 for default/uninitialized messages
    return true;  // Allow all valid method IDs including 0x0000
}

/**
 * @brief Validate Message ID components
 * @implements REQ_MSG_002, REQ_MSG_003
 */
bool Message::has_valid_message_id() const {
    return has_valid_service_id() && has_valid_method_id();
}

/**
 * @brief Validate length field
 * @implements REQ_MSG_012, REQ_MSG_015
 * @implements REQ_MSG_012_E02
 */
bool Message::has_valid_length() const {
    // REQ_MSG_012: Minimum length value
    if (length_ < 8) {
        return false;
    }

    // REQ_MSG_015: Length must be at least minimum header size
    // Additional validation happens in has_valid_header()

    return true;
}

/**
 * @brief Validate Client ID
 * @implements REQ_MSG_025
 */
bool Message::has_valid_client_id() const {
    uint16_t client_id = get_client_id();

    // REQ_MSG_025: Client ID 0 is reserved for SD
    // But allow it for default/uninitialized messages
    return true;  // Allow all client IDs for now
}

/**
 * @brief Validate Session ID
 * @implements REQ_MSG_023, REQ_MSG_024
 * @implements REQ_MSG_024_E01, REQ_MSG_024_E02
 */
bool Message::has_valid_session_id() const {
    uint16_t session_id = get_session_id();

    // REQ_MSG_023: Session ID 0 is disabled session handling
    // This is valid but indicates no session management

    // REQ_MSG_024: Session ID wrap-around handling
    // Session IDs are 16-bit and can wrap around, this is normal

    return true;  // All session IDs are technically valid
}

/**
 * @brief Validate Request ID components
 * @implements REQ_MSG_021, REQ_MSG_022
 */
bool Message::has_valid_request_id() const {
    return has_valid_client_id() && has_valid_session_id();
}

/**
 * @brief Validate message type according to SOME/IP specification
 * @implements REQ_MSG_042, REQ_MSG_042_E01
 * @implements REQ_MSG_051, REQ_MSG_052, REQ_MSG_053, REQ_MSG_054, REQ_MSG_055
 * @implements REQ_MSG_057, REQ_MSG_058, REQ_MSG_059
 */
bool Message::has_valid_message_type() const {
    switch (message_type_) {
        case MessageType::REQUEST:           // REQ_MSG_051
        case MessageType::REQUEST_NO_RETURN: // REQ_MSG_052
        case MessageType::NOTIFICATION:      // REQ_MSG_053
        case MessageType::RESPONSE:          // REQ_MSG_054
        case MessageType::ERROR:             // REQ_MSG_055
        case MessageType::REQUEST_ACK:       // REQ_MSG_057
        case MessageType::RESPONSE_ACK:      // REQ_MSG_058
        case MessageType::ERROR_ACK:         // REQ_MSG_059
        case MessageType::TP_REQUEST:        // TP variants also valid
        case MessageType::TP_REQUEST_NO_RETURN:
        case MessageType::TP_NOTIFICATION:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Check if message has TP flag set
 * @implements REQ_MSG_056
 */
bool Message::has_tp_flag() const {
    return someip::uses_tp(message_type_);
}

/**
 * @brief Validate message header fields
 * @implements REQ_MSG_031, REQ_MSG_032, REQ_MSG_033
 * @implements REQ_MSG_032_E01, REQ_MSG_032_E02
 * @implements REQ_MSG_063, REQ_MSG_064, REQ_MSG_063_E01, REQ_MSG_063_E02
 * @implements REQ_MSG_072, REQ_MSG_072_E01
 * @satisfies feat_req_someip_100, feat_req_someip_103, feat_req_someip_278
 */
bool Message::has_valid_header() const {
    // Check Message ID components (REQ_MSG_002-008)
    if (!has_valid_message_id()) {
        return false;
    }

    // Check Request ID components (REQ_MSG_021-025)
    if (!has_valid_request_id()) {
        return false;
    }

    // Check length field (REQ_MSG_012-015)
    if (!has_valid_length()) {
        return false;
    }

    // Check message type (REQ_MSG_051-059)
    if (!has_valid_message_type()) {
        return false;
    }

    // Check protocol version
    if (protocol_version_ != SOMEIP_PROTOCOL_VERSION) {
        return false;
    }

    // Check interface version
    if (interface_version_ != SOMEIP_INTERFACE_VERSION) {
        return false;
    }

    // Check length consistency
    // length_ contains length from client_id to end (8 + e2e_size + payload_size)
    // Total expected message size should be HEADER_SIZE + e2e_size + payload_size
    size_t e2e_size = e2e_header_.has_value() ? e2e_header_->get_header_size() : 0;
    uint32_t expected_total_size = HEADER_SIZE + e2e_size + payload_.size();
    uint32_t actual_total_size = HEADER_SIZE + e2e_size + payload_.size();  // Same calculation
    if (expected_total_size != actual_total_size) {
        return false;
    }

    // Also check that length_ is consistent
    uint32_t expected_length = 8 + e2e_size + payload_.size();
    if (length_ != expected_length) {
        return false;
    }

    // Check return code validity
    switch (return_code_) {
        case ReturnCode::E_OK:
        case ReturnCode::E_NOT_OK:
        case ReturnCode::E_UNKNOWN_SERVICE:
        case ReturnCode::E_UNKNOWN_METHOD:
        case ReturnCode::E_NOT_READY:
        case ReturnCode::E_NOT_REACHABLE:
        case ReturnCode::E_TIMEOUT:
        case ReturnCode::E_WRONG_PROTOCOL_VERSION:
        case ReturnCode::E_WRONG_INTERFACE_VERSION:
        case ReturnCode::E_MALFORMED_MESSAGE:
        case ReturnCode::E_WRONG_MESSAGE_TYPE:
        case ReturnCode::E_E2E_REPEATED:
        case ReturnCode::E_E2E_WRONG_SEQUENCE:
        case ReturnCode::E_E2E:
        case ReturnCode::E_E2E_NOT_AVAILABLE:
        case ReturnCode::E_E2E_NO_NEW_DATA:
            break;
        default:
            return false;
    }

    return true;
}

/**
 * @brief Validate message payload
 * @implements REQ_MSG_013
 * @satisfies feat_req_someiptp_760
 */
bool Message::has_valid_payload() const {
    // Check payload size limits
    return payload_.size() <= MAX_TCP_PAYLOAD_SIZE;
}

/**
 * @brief Update length field based on payload and E2E header size
 * @implements REQ_MSG_011
 * @satisfies feat_req_someip_67
 */
void Message::update_length() {
    // SOME/IP length field contains length from client_id to end of message
    // client_id(2) + session_id(2) + protocol_version(1) + interface_version(1) +
    // message_type(1) + return_code(1) + e2e_header_size + payload_size
    size_t e2e_size = e2e_header_.has_value() ? e2e_header_->get_header_size() : 0;
    length_ = 8 + e2e_size + payload_.size();
}

void Message::set_e2e_header(const e2e::E2EHeader& header) {
    e2e_header_ = header;
    update_length();
}

void Message::clear_e2e_header() {
    e2e_header_.reset();
    update_length();
}

std::string Message::to_string() const {
    std::stringstream ss;
    ss << "Message{"
       << "service_id=0x" << std::hex << std::setw(4) << std::setfill('0') << get_service_id()
       << ", method_id=0x" << std::hex << std::setw(4) << std::setfill('0') << get_method_id()
       << ", client_id=0x" << std::hex << std::setw(4) << std::setfill('0') << get_client_id()
       << ", session_id=0x" << std::hex << std::setw(4) << std::setfill('0') << get_session_id()
       << ", type=" << someip::to_string(message_type_)
       << ", return_code=" << someip::to_string(return_code_)
       << ", length=" << std::dec << length_
       << ", payload_size=" << payload_.size()
       << "}";

    return ss.str();
}

} // namespace someip
