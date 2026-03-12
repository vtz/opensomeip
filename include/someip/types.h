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

#ifndef SOMEIP_TYPES_H
#define SOMEIP_TYPES_H

#include <cstdint>
#include <vector>
#include <string>

namespace someip {

/**
 * @brief SOME/IP type definitions
 */

/**
 * @brief SOME/IP protocol version
 */
static constexpr uint8_t SOMEIP_PROTOCOL_VERSION = 0x01;

/**
 * @brief SOME/IP interface version
 */
static constexpr uint8_t SOMEIP_INTERFACE_VERSION = 0x01;

/** @implements REQ_MSG_112 */
static constexpr uint16_t SOMEIP_SD_SERVICE_ID = 0xFFFF;

/**
 * @brief SOME/IP Service Discovery method ID
 */
static constexpr uint16_t SOMEIP_SD_METHOD_ID = 0x8100;

/**
 * @brief SOME/IP Service Discovery client ID
 */
static constexpr uint16_t SOMEIP_SD_CLIENT_ID = 0x0000;

/**
 * @brief SOME/IP Service Discovery protocol version
 */
static constexpr uint8_t SOMEIP_SD_PROTOCOL_VERSION = 0x01;

/**
 * @brief SOME/IP Service Discovery interface version
 */
static constexpr uint8_t SOMEIP_SD_INTERFACE_VERSION = 0x01;

/**
 * @brief SOME/IP Service Discovery message type
 */
static constexpr uint8_t SOMEIP_SD_MESSAGE_TYPE = 0x02;

/**
 * @brief SOME/IP Service Discovery return code
 */
static constexpr uint8_t SOMEIP_SD_RETURN_CODE = 0x00;

/** @implements REQ_MSG_111 */
struct MessageId {
    uint16_t service_id{0};
    uint16_t method_id{0};

    MessageId() = default;
    MessageId(uint16_t service, uint16_t method) : service_id(service), method_id(method) {}

    uint32_t to_uint32() const {
        return (static_cast<uint32_t>(service_id) << 16) | method_id;
    }

    static MessageId from_uint32(uint32_t value) {
        return MessageId(static_cast<uint16_t>(value >> 16), static_cast<uint16_t>(value & 0xFFFF));
    }

    bool operator==(const MessageId& other) const {
        return service_id == other.service_id && method_id == other.method_id;
    }

    bool operator!=(const MessageId& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Request ID structure combining Client ID and Session ID
 */
struct RequestId {
    uint16_t client_id{0};
    uint16_t session_id{0};

    RequestId() = default;
    RequestId(uint16_t client, uint16_t session) : client_id(client), session_id(session) {}

    uint32_t to_uint32() const {
        return (static_cast<uint32_t>(client_id) << 16) | session_id;
    }

    static RequestId from_uint32(uint32_t value) {
        return RequestId(static_cast<uint16_t>(value >> 16), static_cast<uint16_t>(value & 0xFFFF));
    }

    bool operator==(const RequestId& other) const {
        return client_id == other.client_id && session_id == other.session_id;
    }

    bool operator!=(const RequestId& other) const {
        return !(*this == other);
    }
};

/**
 * @brief SOME/IP message types
 */
#ifdef ERROR
#undef ERROR
#endif
enum class MessageType : uint8_t {
    REQUEST = 0x00,
    REQUEST_NO_RETURN = 0x01,
    NOTIFICATION = 0x02,
    REQUEST_ACK = 0x40,
    RESPONSE = 0x80,
    ERROR = 0x81,
    RESPONSE_ACK = 0xC0,
    ERROR_ACK = 0xC1,
    TP_REQUEST = 0x20,
    TP_REQUEST_NO_RETURN = 0x21,
    TP_NOTIFICATION = 0x22
};

/** @implements REQ_MSG_131 */
enum class ReturnCode : uint8_t {
    E_OK = 0x00,
    E_NOT_OK = 0x01,
    E_UNKNOWN_SERVICE = 0x02,
    E_UNKNOWN_METHOD = 0x03,
    E_NOT_READY = 0x04,
    E_NOT_REACHABLE = 0x05,
    E_TIMEOUT = 0x06,
    E_WRONG_PROTOCOL_VERSION = 0x07,
    E_WRONG_INTERFACE_VERSION = 0x08,
    E_MALFORMED_MESSAGE = 0x09,
    E_WRONG_MESSAGE_TYPE = 0x0A,
    E_E2E_REPEATED = 0x0B,
    E_E2E_WRONG_SEQUENCE = 0x0C,
    E_E2E = 0x0D,
    E_E2E_NOT_AVAILABLE = 0x0E,
    E_E2E_NO_NEW_DATA = 0x0F
};

/**
 * @brief Convert message type to string
 */
std::string to_string(MessageType type);

/**
 * @brief Convert return code to string
 */
std::string to_string(ReturnCode code);

/**
 * @brief Check if message type indicates a request
 */
bool is_request(MessageType type);

/**
 * @brief Check if message type indicates a response
 */
bool is_response(MessageType type);

/**
 * @brief Check if message type uses TP (Transport Protocol)
 */
bool uses_tp(MessageType type);

/**
 * @brief Get the ACK variant of a message type
 */
MessageType get_ack_type(MessageType type);

/**
 * @brief Check if return code indicates success
 */
bool is_success(ReturnCode code);

} // namespace someip

#endif // SOMEIP_TYPES_H
