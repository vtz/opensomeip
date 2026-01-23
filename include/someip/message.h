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

#ifndef SOMEIP_MESSAGE_H
#define SOMEIP_MESSAGE_H

#include "someip/types.h"
#include "e2e/e2e_header.h"
#include <vector>
#include <memory>
#include <chrono>
#include <optional>

namespace someip {

/**
 * @brief SOME/IP message structure
 *
 * This class represents a complete SOME/IP message including header and payload.
 * It provides serialization/deserialization capabilities and safety checks.
 */
class Message {
public:
    /**
     * @brief Constructor for creating a new message
     */
    Message();

    /**
     * @brief Constructor for creating a message with specific IDs
     */
    Message(MessageId message_id, RequestId request_id,
            MessageType message_type = MessageType::REQUEST,
            ReturnCode return_code = ReturnCode::E_OK);

    /**
     * @brief Copy constructor
     */
    Message(const Message& other);

    /**
     * @brief Move constructor
     */
    Message(Message&& other) noexcept;

    /**
     * @brief Assignment operator
     */
    Message& operator=(const Message& other);

    /**
     * @brief Move assignment operator
     */
    Message& operator=(Message&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~Message() = default;

    // Header field accessors
    MessageId get_message_id() const { return message_id_; }
    void set_message_id(MessageId id) { message_id_ = id; }

    uint32_t get_length() const { return length_; }
    void set_length(uint32_t length) { length_ = length; }

    RequestId get_request_id() const { return request_id_; }
    void set_request_id(RequestId id) { request_id_ = id; }

    uint8_t get_protocol_version() const { return protocol_version_; }
    void set_protocol_version(uint8_t version) { protocol_version_ = version; }

    uint8_t get_interface_version() const { return interface_version_; }
    void set_interface_version(uint8_t version) { interface_version_ = version; }

    MessageType get_message_type() const { return message_type_; }
    void set_message_type(MessageType type) { message_type_ = type; }

    ReturnCode get_return_code() const { return return_code_; }
    void set_return_code(ReturnCode code) { return_code_ = code; }

    // Payload accessors
    const std::vector<uint8_t>& get_payload() const { return payload_; }
    void set_payload(const std::vector<uint8_t>& payload) { payload_ = payload; update_length(); }
    void set_payload(std::vector<uint8_t>&& payload) { payload_ = std::move(payload); update_length(); }

    // Service and method ID convenience accessors
    uint16_t get_service_id() const { return message_id_.service_id; }
    void set_service_id(uint16_t service_id) { message_id_.service_id = service_id; }

    uint16_t get_method_id() const { return message_id_.method_id; }
    void set_method_id(uint16_t method_id) { message_id_.method_id = method_id; }

    uint16_t get_client_id() const { return request_id_.client_id; }
    void set_client_id(uint16_t client_id) { request_id_.client_id = client_id; }

    uint16_t get_session_id() const { return request_id_.session_id; }
    void set_session_id(uint16_t session_id) { request_id_.session_id = session_id; }

    // Serialization methods
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    // Validation methods
    bool is_valid() const;
    bool has_valid_header() const;
    bool has_valid_payload() const;

    // Component validation methods
    bool has_valid_message_id() const;
    bool has_valid_service_id() const;
    bool has_valid_method_id() const;
    bool has_valid_request_id() const;
    bool has_valid_client_id() const;
    bool has_valid_session_id() const;
    bool has_valid_length() const;

    // Utility methods
    size_t get_total_size() const {
        size_t e2e_size = e2e_header_.has_value() ? e2e_header_->get_header_size() : 0;
        return HEADER_SIZE + e2e_size + payload_.size();
    }
    static size_t get_header_size() { return HEADER_SIZE; }
    bool is_request() const { return someip::is_request(message_type_); }
    bool is_response() const { return someip::is_response(message_type_); }
    bool uses_tp() const { return someip::uses_tp(message_type_); }
    bool is_success() const { return someip::is_success(return_code_); }

    // Timestamp for diagnostics
    std::chrono::steady_clock::time_point get_timestamp() const { return timestamp_; }
    void update_timestamp() { timestamp_ = std::chrono::steady_clock::now(); }

    // E2E protection support
    bool has_e2e_header() const { return e2e_header_.has_value(); }
    void set_e2e_header(const e2e::E2EHeader& header);
    std::optional<e2e::E2EHeader> get_e2e_header() const { return e2e_header_; }
    void clear_e2e_header();

    // String representation for debugging
    std::string to_string() const;

private:
    // Header fields (16 bytes total)
    MessageId message_id_;           // 4 bytes
    uint32_t length_;                // 4 bytes (includes 8-byte header)
    RequestId request_id_;           // 4 bytes
    uint8_t protocol_version_;       // 1 byte
    uint8_t interface_version_;      // 1 byte
    MessageType message_type_;       // 1 byte
    ReturnCode return_code_;         // 1 byte

    // Payload
    std::vector<uint8_t> payload_;

    // E2E protection header (optional)
    std::optional<e2e::E2EHeader> e2e_header_;

    // Metadata
    std::chrono::steady_clock::time_point timestamp_;

    // Constants
    static constexpr size_t HEADER_SIZE = 16;
    static constexpr size_t MIN_MESSAGE_SIZE = HEADER_SIZE;
    static constexpr size_t DEFAULT_MAX_PAYLOAD_SIZE = 1400; // Ethernet MTU minus headers
    static constexpr size_t MAX_TCP_PAYLOAD_SIZE = 65535; // Much larger for TCP

    // Helper methods
    void update_length();
    bool validate_header() const;
    bool validate_payload() const;
};

// Type aliases for convenience
using MessagePtr = std::shared_ptr<Message>;
using MessageConstPtr = std::shared_ptr<const Message>;

} // namespace someip

#endif // SOMEIP_MESSAGE_H
