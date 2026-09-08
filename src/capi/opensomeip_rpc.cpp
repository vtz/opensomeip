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
 * @file opensomeip_rpc.cpp
 * @brief C API wrappers for RPC client/server operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_004, REQ_CAPI_007, REQ_CAPI_010
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_server.h"
#include <cstring>

struct opensomeip_rpc_client_s {
    someip::rpc::RpcClient client;
    explicit opensomeip_rpc_client_s(uint16_t id) : client(id) {}
};

struct opensomeip_rpc_server_s {
    someip::rpc::RpcServer server;
    explicit opensomeip_rpc_server_s(uint16_t id) : server(id) {}
};

extern "C" opensomeip_result_t opensomeip_rpc_client_create(opensomeip_rpc_client_t** out,
                                                             uint16_t client_id) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_rpc_client_s(client_id);
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_client_destroy(opensomeip_rpc_client_t* c) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete c; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_client_initialize(opensomeip_rpc_client_t* c) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return c->client.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_client_shutdown(opensomeip_rpc_client_t* c) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { c->client.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_client_call_sync(opensomeip_rpc_client_t* c,
                                                                uint16_t service_id,
                                                                uint16_t method_id,
                                                                const uint8_t* input_data,
                                                                size_t input_len,
                                                                uint8_t* output_data,
                                                                size_t* output_len,
                                                                uint32_t timeout_ms) {
    if (!c || !output_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (input_len > 0 && !input_data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::platform::ByteBuffer params(input_data, input_data + input_len);
        someip::rpc::RpcTimeout timeout;
        timeout.request_timeout = std::chrono::milliseconds(timeout_ms);
        timeout.response_timeout = std::chrono::milliseconds(timeout_ms);
        auto result = c->client.call_method_sync(service_id, method_id, params, timeout);

        if (result.result != someip::rpc::RpcResult::SUCCESS) {
            switch (result.result) {
                case someip::rpc::RpcResult::TIMEOUT: return OPENSOMEIP_RESULT_TIMEOUT;
                case someip::rpc::RpcResult::NETWORK_ERROR: return OPENSOMEIP_RESULT_NETWORK_ERROR;
                case someip::rpc::RpcResult::METHOD_NOT_FOUND: return OPENSOMEIP_RESULT_INVALID_METHOD_ID;
                case someip::rpc::RpcResult::SERVICE_NOT_AVAILABLE: return OPENSOMEIP_RESULT_SERVICE_UNAVAILABLE;
                default: return OPENSOMEIP_RESULT_INTERNAL_ERROR;
            }
        }

        if (*output_len < result.return_values.size()) {
            *output_len = result.return_values.size();
            return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
        }
        if (!result.return_values.empty() && output_data) {
            std::memcpy(output_data, result.return_values.data(), result.return_values.size());
        }
        *output_len = result.return_values.size();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_client_call_async(opensomeip_rpc_client_t* c,
                                                                 uint16_t service_id,
                                                                 uint16_t method_id,
                                                                 const uint8_t* input_data,
                                                                 size_t input_len,
                                                                 opensomeip_rpc_callback_t callback,
                                                                 void* user_data,
                                                                 uint32_t timeout_ms,
                                                                 uint32_t* out_handle) {
    if (!c || !callback || !out_handle) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (input_len > 0 && !input_data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::platform::ByteBuffer params(input_data, input_data + input_len);
        someip::rpc::RpcTimeout timeout;
        timeout.request_timeout = std::chrono::milliseconds(timeout_ms);
        timeout.response_timeout = std::chrono::milliseconds(timeout_ms);

        auto cb = callback;
        auto ud = user_data;
        auto handle = c->client.call_method_async(
            service_id, method_id, params,
            [cb, ud](const someip::rpc::RpcResponse& resp) {
                opensomeip_result_t res = OPENSOMEIP_RESULT_SUCCESS;
                if (resp.result != someip::rpc::RpcResult::SUCCESS) {
                    res = OPENSOMEIP_RESULT_INTERNAL_ERROR;
                }
                cb(res, resp.return_values.data(), resp.return_values.size(), ud);
            },
            timeout
        );
        *out_handle = handle;
        return (handle != 0) ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_client_cancel(opensomeip_rpc_client_t* c, uint32_t handle) {
    if (!c) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return c->client.cancel_call(handle) ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_server_create(opensomeip_rpc_server_t** out,
                                                             uint16_t service_id) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_rpc_server_s(service_id);
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_server_destroy(opensomeip_rpc_server_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete s; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_server_initialize(opensomeip_rpc_server_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return s->server.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_server_shutdown(opensomeip_rpc_server_t* s) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { s->server.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_server_register_method(opensomeip_rpc_server_t* s,
                                                                      uint16_t method_id,
                                                                      opensomeip_method_handler_t handler,
                                                                      void* user_data) {
    if (!s || !handler) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto h = handler;
        auto ud = user_data;
        someip::rpc::MethodHandler cpp_handler =
            [h, ud](uint16_t client_id, uint16_t session_id,
                     const someip::platform::ByteBuffer& input,
                     someip::platform::ByteBuffer& output) -> someip::rpc::RpcResult {
                uint8_t out_buf[4096];
                size_t out_len = sizeof(out_buf);
                auto rc = h(client_id, session_id, input.data(), input.size(),
                            out_buf, &out_len, ud);
                if (rc == OPENSOMEIP_RESULT_SUCCESS) {
                    output.resize(out_len);
                    if (out_len > 0) {
                        std::memcpy(output.data(), out_buf, out_len);
                    }
                    return someip::rpc::RpcResult::SUCCESS;
                }
                return someip::rpc::RpcResult::INTERNAL_ERROR;
            };
        return s->server.register_method(method_id, std::move(cpp_handler))
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INVALID_STATE;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_rpc_server_unregister_method(opensomeip_rpc_server_t* s,
                                                                        uint16_t method_id) {
    if (!s) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return s->server.unregister_method(method_id)
            ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
