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

#include "someip/types.h"
#include <unordered_map>

namespace someip {

/**
 * @brief SOME/IP type conversions
 * @implements REQ_ARCH_001
 * @satisfies feat_req_someip_542
 * @satisfies feat_req_someip_543
 */

std::string to_string(MessageType type) {
    static const std::unordered_map<MessageType, std::string> type_strings = {
        {MessageType::REQUEST, "REQUEST"},
        {MessageType::REQUEST_NO_RETURN, "REQUEST_NO_RETURN"},
        {MessageType::NOTIFICATION, "NOTIFICATION"},
        {MessageType::REQUEST_ACK, "REQUEST_ACK"},
        {MessageType::RESPONSE, "RESPONSE"},
        {MessageType::ERROR, "ERROR"},
        {MessageType::RESPONSE_ACK, "RESPONSE_ACK"},
        {MessageType::ERROR_ACK, "ERROR_ACK"},
        {MessageType::TP_REQUEST, "TP_REQUEST"},
        {MessageType::TP_REQUEST_NO_RETURN, "TP_REQUEST_NO_RETURN"},
        {MessageType::TP_NOTIFICATION, "TP_NOTIFICATION"}
    };

    auto it = type_strings.find(type);
    return (it != type_strings.end()) ? it->second : "UNKNOWN_MESSAGE_TYPE";
}

std::string to_string(ReturnCode code) {
    static const std::unordered_map<ReturnCode, std::string> code_strings = {
        {ReturnCode::E_OK, "E_OK"},
        {ReturnCode::E_NOT_OK, "E_NOT_OK"},
        {ReturnCode::E_UNKNOWN_SERVICE, "E_UNKNOWN_SERVICE"},
        {ReturnCode::E_UNKNOWN_METHOD, "E_UNKNOWN_METHOD"},
        {ReturnCode::E_NOT_READY, "E_NOT_READY"},
        {ReturnCode::E_NOT_REACHABLE, "E_NOT_REACHABLE"},
        {ReturnCode::E_TIMEOUT, "E_TIMEOUT"},
        {ReturnCode::E_WRONG_PROTOCOL_VERSION, "E_WRONG_PROTOCOL_VERSION"},
        {ReturnCode::E_WRONG_INTERFACE_VERSION, "E_WRONG_INTERFACE_VERSION"},
        {ReturnCode::E_MALFORMED_MESSAGE, "E_MALFORMED_MESSAGE"},
        {ReturnCode::E_WRONG_MESSAGE_TYPE, "E_WRONG_MESSAGE_TYPE"},
        {ReturnCode::E_E2E_REPEATED, "E_E2E_REPEATED"},
        {ReturnCode::E_E2E_WRONG_SEQUENCE, "E_E2E_WRONG_SEQUENCE"},
        {ReturnCode::E_E2E, "E_E2E"},
        {ReturnCode::E_E2E_NOT_AVAILABLE, "E_E2E_NOT_AVAILABLE"},
        {ReturnCode::E_E2E_NO_NEW_DATA, "E_E2E_NO_NEW_DATA"}
    };

    auto it = code_strings.find(code);
    return (it != code_strings.end()) ? it->second : "UNKNOWN_RETURN_CODE";
}

bool is_request(MessageType type) {
    return type == MessageType::REQUEST ||
           type == MessageType::REQUEST_NO_RETURN ||
           type == MessageType::TP_REQUEST ||
           type == MessageType::TP_REQUEST_NO_RETURN;
}

bool is_response(MessageType type) {
    return type == MessageType::RESPONSE ||
           type == MessageType::ERROR ||
           type == MessageType::RESPONSE_ACK ||
           type == MessageType::ERROR_ACK;
}

bool uses_tp(MessageType type) {
    return type == MessageType::TP_REQUEST ||
           type == MessageType::TP_REQUEST_NO_RETURN ||
           type == MessageType::TP_NOTIFICATION;
}

MessageType get_ack_type(MessageType type) {
    switch (type) {
        case MessageType::REQUEST:
            return MessageType::REQUEST_ACK;
        case MessageType::RESPONSE:
            return MessageType::RESPONSE_ACK;
        case MessageType::ERROR:
            return MessageType::ERROR_ACK;
        default:
            return type; // No ACK variant
    }
}

bool is_success(ReturnCode code) {
    return code == ReturnCode::E_OK;
}

} // namespace someip
