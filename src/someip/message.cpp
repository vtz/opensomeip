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
      timestamp_(other.timestamp_) {
    // Length is copied as-is for copy constructor
}

Message::Message(Message&& other) noexcept
    : message_id_(other.message_id_),
      length_(8 + other.payload_.size()),  // Length for moved-to object
      request_id_(other.request_id_),
      protocol_version_(other.protocol_version_),
      interface_version_(other.interface_version_),
      message_type_(other.message_type_),
      return_code_(other.return_code_),
      payload_(std::move(other.payload_)),  // Move the payload
      timestamp_(other.timestamp_) {
    // Invalidate the moved-from object (safety-critical design: moved-from messages are invalid)
    other.interface_version_ = 0xFF;
    other.length_ = 8;  // Reset length for empty payload
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
        timestamp_ = other.timestamp_;
    }
    return *this;
}

Message& Message::operator=(Message&& other) noexcept {
    if (this != &other) {
        message_id_ = other.message_id_;
        length_ = 8 + other.payload_.size();  // Length for moved-to object
        request_id_ = other.request_id_;
        protocol_version_ = other.protocol_version_;
        interface_version_ = SOMEIP_INTERFACE_VERSION;  // Valid interface version for moved-to object
        message_type_ = other.message_type_;
        return_code_ = other.return_code_;
        payload_ = std::move(other.payload_);  // Move the payload
        timestamp_ = other.timestamp_;

        // Invalidate the moved-from object
        other.interface_version_ = 0xFF;
        other.length_ = 8;  // Reset length for empty payload
    }
    return *this;
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(get_total_size());

    // Serialize header in big-endian format (network byte order)
    uint32_t message_id_be = htonl(message_id_.to_uint32());
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&message_id_be),
                reinterpret_cast<uint8_t*>(&message_id_be) + sizeof(uint32_t));

    uint32_t length_be = htonl(length_);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&length_be),
                reinterpret_cast<uint8_t*>(&length_be) + sizeof(uint32_t));

    uint32_t request_id_be = htonl(request_id_.to_uint32());
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&request_id_be),
                reinterpret_cast<uint8_t*>(&request_id_be) + sizeof(uint32_t));

    data.push_back(protocol_version_);
    data.push_back(interface_version_);
    data.push_back(static_cast<uint8_t>(message_type_));
    data.push_back(static_cast<uint8_t>(return_code_));

    // Append payload
    data.insert(data.end(), payload_.begin(), payload_.end());

    return data;
}

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

    // Calculate expected payload size
    // length_ contains length from client_id to end: 8 + payload_size
    if (length_ < 8) {
        return false;  // Invalid length: must be at least 8 for header
    }
    size_t expected_payload_size = length_ - 8;
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

bool Message::is_valid() const {
    return has_valid_header() && has_valid_payload();
}

bool Message::has_valid_header() const {
    // Check protocol version
    if (protocol_version_ != SOMEIP_PROTOCOL_VERSION) {
        return false;
    }

    // Check interface version
    if (interface_version_ != SOMEIP_INTERFACE_VERSION) {
        return false;
    }

    // Check length consistency
    // length_ contains length from client_id to end (8 + payload_size)
    // Total expected message size should be 8 + length_
    uint32_t expected_total_size = 8 + length_;
    uint32_t actual_total_size = HEADER_SIZE + payload_.size();
    if (expected_total_size != actual_total_size) {
        return false;
    }

    // Check message type validity
    switch (message_type_) {
        case MessageType::REQUEST:
        case MessageType::REQUEST_NO_RETURN:
        case MessageType::NOTIFICATION:
        case MessageType::REQUEST_ACK:
        case MessageType::RESPONSE:
        case MessageType::ERROR:
        case MessageType::RESPONSE_ACK:
        case MessageType::ERROR_ACK:
        case MessageType::TP_REQUEST:
        case MessageType::TP_REQUEST_NO_RETURN:
        case MessageType::TP_NOTIFICATION:
            break;
        default:
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

bool Message::has_valid_payload() const {
    // Check payload size limits
    return payload_.size() <= MAX_TCP_PAYLOAD_SIZE;
}

void Message::update_length() {
    // SOME/IP length field contains length from client_id to end of message
    // client_id(2) + session_id(2) + protocol_version(1) + interface_version(1) +
    // message_type(1) + return_code(1) + payload_size = 8 + payload_size
    length_ = 8 + payload_.size();
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
