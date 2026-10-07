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
#include "someip/payload_view.h"
#include "e2e/e2e_header.h"
#include "e2e/e2e_layout.h"
#include "common/result.h"
#include "platform/buffer_pool.h"
#include "platform/intrusive_ptr.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
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
    const platform::ByteBuffer& get_payload() const { return payload_; }
    PayloadView payload_view() const { return PayloadView(payload_); }
    void set_payload(const platform::ByteBuffer& payload) { payload_ = payload; update_length(); }
    void set_payload(platform::ByteBuffer&& payload) { payload_ = std::move(payload); update_length(); }
    void set_payload(const uint8_t* data, size_t size) {
        payload_.resize(size);
        if (data != nullptr && size > 0) { std::memcpy(payload_.data(), data, size); }
        update_length();
    }

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
    platform::ByteBuffer serialize() const;
    bool deserialize(const platform::ByteBuffer& data, bool expect_e2e = false);
    bool deserialize(const uint8_t* data, size_t size, bool expect_e2e = false);

    /**
     * @brief Deserialize with a structured local Result (not an on-wire ReturnCode)
     *
     * Parsed header fields that were read before a failure remain on this
     * object for receiver diagnostics. deserialize() is a bool wrapper around
     * this method.
     *
     * expect_e2e == true selects the default layout only: Offset 64 bits and
     * a 12-byte header immediately after Return Code. Any other layout must
     * be passed as E2EParseOptions. Offset and header size are not discovered
     * from the datagram.
     */
    Result try_deserialize(const platform::ByteBuffer& data, bool expect_e2e = false);
    Result try_deserialize(const uint8_t* data, size_t size, bool expect_e2e = false);
    Result try_deserialize(const platform::ByteBuffer& data, const e2e::E2EParseOptions& options);
    Result try_deserialize(const uint8_t* data, size_t size, const e2e::E2EParseOptions& options);

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
    bool has_valid_message_type() const;
    bool has_tp_flag() const;

    // Utility methods
    size_t get_total_size() const { return HEADER_SIZE + e2e_on_wire_size() + payload_.size(); }
    static size_t get_header_size() { return HEADER_SIZE; }
    bool is_request() const { return someip::is_request(message_type_); }
    bool is_response() const { return someip::is_response(message_type_); }
    bool uses_tp() const { return someip::uses_tp(message_type_); }
    bool is_success() const { return someip::is_success(return_code_); }

    // Timestamp for diagnostics
    std::chrono::steady_clock::time_point get_timestamp() const { return timestamp_; }
    void update_timestamp() { timestamp_ = std::chrono::steady_clock::now(); }

    // E2E protection support.
    // Profile bytes live in a fixed buffer. The 12-byte E2EHeader view is
    // available only when the stored header is exactly that size.
    // set_e2e_header() installs the default layout (Offset 64, no prefix).
    bool has_e2e_header() const { return e2e_header_size_ > 0; }
    void set_e2e_header(const e2e::E2EHeader& header);
    std::optional<e2e::E2EHeader> get_e2e_header() const;
    void clear_e2e_header();

    /**
     * @brief Store the unprotected bytes that sit between Return Code and the E2E header.
     *
     * size must equal (offset_bits / 8) - 8. Those bytes are not part of
     * get_payload() and are not covered by the basic profile CRC. They are
     * included in Length once a header is installed.
     */
    Result set_e2e_unprotected_prefix(uint32_t offset_bits, const uint8_t* data, size_t size);

    /**
     * @brief Install profile header bytes at a resolved Offset.
     *
     * prefix_size must equal the prefix implied by offset_bits. Sources must
     * not be required to outlive the call; overlapping the message's own
     * E2E storage is safe.
     */
    Result set_e2e_profile_bytes(uint32_t offset_bits, const uint8_t* header, size_t header_size,
                                 const uint8_t* prefix, size_t prefix_size);

    uint32_t e2e_offset_bits() const { return e2e_offset_bits_; }
    size_t e2e_header_size() const { return e2e_header_size_; }
    const uint8_t* e2e_header_bytes() const { return e2e_header_bytes_.data(); }
    size_t e2e_unprotected_prefix_size() const { return e2e_prefix_size_; }
    const uint8_t* e2e_unprotected_prefix() const { return e2e_prefix_bytes_.data(); }

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
    platform::ByteBuffer payload_;

    // E2E region. Header size 0 means no header. Prefix bytes are the
    // unprotected gap after Return Code and are emitted only with a header.
    uint32_t e2e_offset_bits_{e2e::E2EConfig::DEFAULT_OFFSET_BITS};
    size_t e2e_header_size_{0};
    size_t e2e_prefix_size_{0};
    std::array<uint8_t, e2e::kMaxE2EHeaderSize> e2e_header_bytes_{};
    std::array<uint8_t, e2e::kMaxE2EPrefixSize> e2e_prefix_bytes_{};

    // Metadata
    std::chrono::steady_clock::time_point timestamp_;

    mutable std::atomic<uint16_t> ref_count_{0};

    friend void intrusive_ptr_add_ref(const Message* p);
    friend void intrusive_ptr_release(const Message* p);

    // Constants
    static constexpr size_t HEADER_SIZE = 16;
    static constexpr size_t MIN_MESSAGE_SIZE = HEADER_SIZE;
    static constexpr size_t DEFAULT_MAX_PAYLOAD_SIZE = 1400; // Ethernet MTU minus headers
    static constexpr size_t MAX_TCP_PAYLOAD_SIZE = 65535; // Much larger for TCP

    // Helper methods
    void update_length();
    bool validate_header() const;
    bool validate_payload() const;
    Result header_validation_result() const;
    Result validation_result() const;
    size_t e2e_on_wire_size() const { return e2e_header_size_ == 0 ? 0 : (e2e_prefix_size_ + e2e_header_size_); }
    void copy_e2e_from(const Message& other);
    void reset_e2e();
};

void intrusive_ptr_add_ref(const Message* p);
void intrusive_ptr_release(const Message* p);

}  // namespace someip

// MessagePtr typedef is backend-specific; resolved by include-path shadowing.
#include "platform/message_ptr.h"

#endif // SOMEIP_MESSAGE_H
