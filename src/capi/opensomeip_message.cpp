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

/**
 * @file opensomeip_message.cpp
 * @brief C API wrappers for SOME/IP message operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_007, REQ_CAPI_008
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "common/result.h"
#include <cstring>
#include <new>

/** @implements REQ_CAPI_005 */
extern "C" uint32_t opensomeip_capi_version(void) {
    return (OPENSOMEIP_CAPI_VERSION_MAJOR << 16) |
           (OPENSOMEIP_CAPI_VERSION_MINOR << 8)  |
            OPENSOMEIP_CAPI_VERSION_PATCH;
}

extern "C" opensomeip_result_t opensomeip_message_create(opensomeip_message_t** out_msg) {
    if (!out_msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        *out_msg = new opensomeip_message_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) {
        *out_msg = nullptr;
        return OPENSOMEIP_RESULT_INTERNAL_ERROR;
    }
}

extern "C" opensomeip_result_t opensomeip_message_destroy(opensomeip_message_t* msg) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        delete msg;
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) {
        return OPENSOMEIP_RESULT_INTERNAL_ERROR;
    }
}

extern "C" opensomeip_result_t opensomeip_message_set_service_id(opensomeip_message_t* msg, uint16_t service_id) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { msg->msg.set_service_id(service_id); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_service_id(const opensomeip_message_t* msg, uint16_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = msg->msg.get_service_id(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_method_id(opensomeip_message_t* msg, uint16_t method_id) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { msg->msg.set_method_id(method_id); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_method_id(const opensomeip_message_t* msg, uint16_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = msg->msg.get_method_id(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_client_id(opensomeip_message_t* msg, uint16_t client_id) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { msg->msg.set_client_id(client_id); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_client_id(const opensomeip_message_t* msg, uint16_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = msg->msg.get_client_id(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_session_id(opensomeip_message_t* msg, uint16_t session_id) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { msg->msg.set_session_id(session_id); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_session_id(const opensomeip_message_t* msg, uint16_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = msg->msg.get_session_id(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_protocol_version(opensomeip_message_t* msg, uint8_t version) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { msg->msg.set_protocol_version(version); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_protocol_version(const opensomeip_message_t* msg, uint8_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = msg->msg.get_protocol_version(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_interface_version(opensomeip_message_t* msg, uint8_t version) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { msg->msg.set_interface_version(version); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_interface_version(const opensomeip_message_t* msg, uint8_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = msg->msg.get_interface_version(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_message_type(opensomeip_message_t* msg, opensomeip_message_type_t type) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        msg->msg.set_message_type(static_cast<someip::MessageType>(type));
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_message_type(const opensomeip_message_t* msg, opensomeip_message_type_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        *out = static_cast<opensomeip_message_type_t>(msg->msg.get_message_type());
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_return_code(opensomeip_message_t* msg, opensomeip_return_code_t code) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        msg->msg.set_return_code(static_cast<someip::ReturnCode>(code));
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_return_code(const opensomeip_message_t* msg, opensomeip_return_code_t* out) {
    if (!msg || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        *out = static_cast<opensomeip_return_code_t>(msg->msg.get_return_code());
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_set_payload(opensomeip_message_t* msg,
                                                               const uint8_t* data, size_t len) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (len > 0 && !data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        msg->msg.set_payload(data, len);
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_payload(const opensomeip_message_t* msg,
                                                               uint8_t* buf, size_t* out_len) {
    if (!msg || !out_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        const auto& payload = msg->msg.get_payload();
        if (*out_len < payload.size()) {
            *out_len = payload.size();
            return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
        }
        if (!payload.empty()) {
            if (!buf) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
            std::memcpy(buf, payload.data(), payload.size());
        }
        *out_len = payload.size();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_get_payload_length(const opensomeip_message_t* msg,
                                                                      size_t* out_len) {
    if (!msg || !out_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        *out_len = msg->msg.get_payload().size();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_serialize(const opensomeip_message_t* msg,
                                                             uint8_t* buf, size_t* buf_len) {
    if (!msg || !buf_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto data = msg->msg.serialize();
        if (*buf_len < data.size()) {
            *buf_len = data.size();
            return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
        }
        if (!data.empty()) {
            if (!buf) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
            std::memcpy(buf, data.data(), data.size());
        }
        *buf_len = data.size();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_message_deserialize(opensomeip_message_t* msg,
                                                               const uint8_t* data, size_t len) {
    if (!msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (len > 0 && !data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        if (!msg->msg.deserialize(data, len)) {
            return OPENSOMEIP_RESULT_MALFORMED_MESSAGE;
        }
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
