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

#ifndef SOMEIP_COMMON_RESULT_H
#define SOMEIP_COMMON_RESULT_H

#include <cstdint>
#include <string>

namespace someip {

/** @implements REQ_ARCH_004, REQ_ARCH_005, REQ_ARCH_006, REQ_ARCH_007 */
enum class Result : uint8_t {
    // Success codes
    SUCCESS = 0x00,

    // Network-related errors
    NETWORK_ERROR = 0x01,
    NOT_CONNECTED = 0x02,
    CONNECTION_LOST = 0x03,
    CONNECTION_REFUSED = 0x04,
    TIMEOUT = 0x05,
    INVALID_ENDPOINT = 0x06,

    // Protocol errors
    INVALID_MESSAGE = 0x10,
    INVALID_MESSAGE_TYPE = 0x11,
    INVALID_SERVICE_ID = 0x12,
    INVALID_METHOD_ID = 0x13,
    INVALID_PROTOCOL_VERSION = 0x14,
    INVALID_INTERFACE_VERSION = 0x15,
    MALFORMED_MESSAGE = 0x16,

    // Session errors
    INVALID_SESSION_ID = 0x20,
    SESSION_EXPIRED = 0x21,
    SESSION_NOT_FOUND = 0x22,

    // Resource errors
    OUT_OF_MEMORY = 0x30,
    BUFFER_OVERFLOW = 0x31,
    RESOURCE_EXHAUSTED = 0x32,

    // Service discovery errors
    SERVICE_NOT_FOUND = 0x40,
    SERVICE_UNAVAILABLE = 0x41,
    SUBSCRIPTION_FAILED = 0x42,

    // Safety-related errors
    SAFETY_VIOLATION = 0x50,
    FAULT_DETECTED = 0x51,
    RECOVERY_FAILED = 0x52,

    // General errors
    NOT_IMPLEMENTED = 0x60,
    INVALID_ARGUMENT = 0x61,
    PERMISSION_DENIED = 0x62,
    INTERNAL_ERROR = 0x63,
    NOT_INITIALIZED = 0x64,
    INVALID_STATE = 0x65,

    // Unknown/undefined
    UNKNOWN_ERROR = 0xFF
};

/**
 * @brief Convert result code to string representation
 * @param result The result code to convert
 * @return String representation of the result
 */
std::string to_string(Result result);

/**
 * @brief Check if result indicates success
 * @param result The result code to check
 * @return true if result is SUCCESS, false otherwise
 */
inline bool is_success(Result result) {
    return result == Result::SUCCESS;
}

/**
 * @brief Check if result indicates an error
 * @param result The result code to check
 * @return true if result is an error, false otherwise
 */
inline bool is_error(Result result) {
    return result != Result::SUCCESS;
}

} // namespace someip

#endif // SOMEIP_COMMON_RESULT_H
