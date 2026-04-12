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

#ifndef SOMEIP_RPC_SERVER_H
#define SOMEIP_RPC_SERVER_H

#include "rpc/rpc_types.h"
#include <memory>
#include <functional>

namespace someip::rpc {

/**
 * @brief Forward declaration
 */
class RpcServerImpl;

/**
 * @brief Method handler function type
 *
 * Function signature for handling RPC method calls on the server side.
 * Receives method parameters and returns result with output parameters.
 */
using MethodHandler = std::function<RpcResult(
    uint16_t client_id,
    uint16_t session_id,
    const std::vector<uint8_t>& input_params,
    std::vector<uint8_t>& output_params
)>;

/**
 * @brief SOME/IP RPC Server Interface
 *
 * This interface allows applications to register method handlers and respond
 * to incoming RPC method calls from clients.
 */
class RpcServer {
public:
    /**
     * @brief Constructor
     * @param service_id Service identifier this server handles
     */
    explicit RpcServer(uint16_t service_id);

    /**
     * @brief Destructor
     */
    ~RpcServer();

    // Delete copy and move operations
    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;
    RpcServer(RpcServer&&) = delete;
    RpcServer& operator=(RpcServer&&) = delete;

    /**
     * @brief Initialize the RPC server
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the RPC server
     */
    void shutdown();

    /**
     * @brief Register a method handler
     *
     * @param method_id Method identifier
     * @param handler Function to handle method calls
     * @return true if registered successfully, false if method already exists
     */
    bool register_method(MethodId method_id, MethodHandler handler);

    /**
     * @brief Unregister a method handler
     *
     * @param method_id Method identifier to remove
     * @return true if unregistered, false if method not found
     */
    bool unregister_method(MethodId method_id);

    /**
     * @brief Check if method is registered
     *
     * @param method_id Method identifier
     * @return true if method is registered
     */
    bool is_method_registered(MethodId method_id) const;

    /**
     * @brief Get registered method IDs
     *
     * @return Vector of all registered method IDs
     */
    std::vector<MethodId> get_registered_methods() const;

    /**
     * @brief Check if server is initialized and ready
     *
     * @return true if ready to handle RPC calls
     */
    bool is_ready() const;

    /**
     * @brief Get server statistics
     *
     * @return Statistics about handled RPC calls
     */
    struct Statistics {
        uint32_t total_calls_received{0};
        uint32_t successful_calls{0};
        uint32_t failed_calls{0};
        uint32_t method_not_found_errors{0};
        std::chrono::milliseconds average_processing_time{0};
    };
    Statistics get_statistics() const;

private:
    std::unique_ptr<RpcServerImpl> impl_;
};

}  // namespace someip::rpc

#endif // SOMEIP_RPC_SERVER_H
