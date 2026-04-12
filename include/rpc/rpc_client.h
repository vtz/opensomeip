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

#ifndef SOMEIP_RPC_CLIENT_H
#define SOMEIP_RPC_CLIENT_H

#include "rpc/rpc_types.h"
#include <memory>

namespace someip::rpc {

/**
 * @brief Forward declaration
 */
class RpcClientImpl;

/**
 * @brief SOME/IP RPC Client Interface
 *
 * This interface provides synchronous and asynchronous RPC method call capabilities.
 * Applications use this interface to invoke methods on remote SOME/IP services.
 */
class RpcClient {
public:
    /**
     * @brief Constructor
     * @param client_id Unique client identifier
     */
    explicit RpcClient(uint16_t client_id);

    /**
     * @brief Destructor
     */
    ~RpcClient() = default;

    // Delete copy and move operations
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    RpcClient(RpcClient&&) = delete;
    RpcClient& operator=(RpcClient&&) = delete;

    /**
     * @brief Initialize the RPC client
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the RPC client
     */
    void shutdown();

    /**
     * @brief Synchronous RPC method call
     *
     * @param service_id Target service ID
     * @param method_id Method to call
     * @param parameters Serialized method parameters
     * @param timeout Call timeout configuration
     * @return Synchronous result with return values or error
     */
    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const std::vector<uint8_t>& parameters,
                                   const RpcTimeout& timeout = RpcTimeout());

    /**
     * @brief Asynchronous RPC method call
     *
     * @param service_id Target service ID
     * @param method_id Method to call
     * @param parameters Serialized method parameters
     * @param callback Completion callback function
     * @param timeout Call timeout configuration
     * @return Call handle for cancellation, or 0 on failure
     */
    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const std::vector<uint8_t>& parameters,
                                    RpcCallback callback,
                                    const RpcTimeout& timeout = RpcTimeout());

    /**
     * @brief Cancel asynchronous RPC call
     *
     * @param handle Call handle returned by call_method_async
     * @return true if call was cancelled, false if not found or already completed
     */
    bool cancel_call(RpcCallHandle handle);

    /**
     * @brief Check if client is initialized and ready
     *
     * @return true if ready for RPC calls
     */
    bool is_ready() const;

    /**
     * @brief Get client statistics
     *
     * @return Statistics about RPC calls (success rate, timing, etc.)
     */
    struct Statistics {
        uint32_t total_calls{0};
        uint32_t successful_calls{0};
        uint32_t failed_calls{0};
        uint32_t timeout_calls{0};
        std::chrono::milliseconds average_response_time{0};
    };
    Statistics get_statistics() const;

private:
    std::unique_ptr<RpcClientImpl> impl_;
};

}  // namespace someip::rpc

#endif // SOMEIP_RPC_CLIENT_H
