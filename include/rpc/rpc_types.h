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

#ifndef SOMEIP_RPC_TYPES_H
#define SOMEIP_RPC_TYPES_H

#include <cstdint>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace someip::rpc {

/**
 * @brief RPC call result codes
 */
enum class RpcResult : uint8_t {
    SUCCESS,
    TIMEOUT,
    NETWORK_ERROR,
    INVALID_PARAMETERS,
    METHOD_NOT_FOUND,
    SERVICE_NOT_AVAILABLE,
    INTERNAL_ERROR
};

/**
 * @brief RPC call handle for tracking asynchronous calls
 */
using RpcCallHandle = uint32_t;

/**
 * @brief Method ID type for RPC calls
 */
using MethodId = uint16_t;

/**
 * @brief Timeout configuration for RPC calls
 */
struct RpcTimeout {
    std::chrono::milliseconds request_timeout{1000};  // Default 1 second
    std::chrono::milliseconds response_timeout{5000}; // Default 5 seconds
};

/**
 * @brief RPC request context
 */
struct RpcRequest {
    uint16_t service_id;
    MethodId method_id;
    uint16_t client_id;
    uint16_t session_id;
    std::vector<uint8_t> parameters;
    RpcTimeout timeout;

    RpcRequest(uint16_t svc_id, MethodId meth_id, uint16_t cli_id, uint16_t sess_id)
        : service_id(svc_id), method_id(meth_id), client_id(cli_id), session_id(sess_id) {}
};

/**
 * @brief RPC response context
 */
struct RpcResponse {
    uint16_t service_id;
    MethodId method_id;
    uint16_t client_id;
    uint16_t session_id;
    RpcResult result;
    std::vector<uint8_t> return_values;

    RpcResponse(uint16_t svc_id, MethodId meth_id, uint16_t cli_id, uint16_t sess_id, RpcResult res)
        : service_id(svc_id), method_id(meth_id), client_id(cli_id), session_id(sess_id), result(res) {}
};

/**
 * @brief Asynchronous RPC completion callback
 */
using RpcCallback = std::function<void(const RpcResponse&)>;

/**
 * @brief Synchronous RPC call result
 */
struct RpcSyncResult {
    RpcResult result;
    std::vector<uint8_t> return_values;
    std::chrono::milliseconds response_time{0};
};

}  // namespace someip::rpc

#endif // SOMEIP_RPC_TYPES_H
