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

#include "platform/buffer_pool.h"
#include "rpc/rpc_types.h"
#include "transport/endpoint.h"
#include "transport/message_rejection.h"
#include "transport/transport.h"

#ifdef SOMEIP_STATIC_ALLOC
#include "static_config.h"
#else
#include <memory>
#endif

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
     * @param interface_version Service major / Interface Version stamped on outgoing calls (default 0x01)
     * @param local_bind Local UDP bind endpoint; default is ephemeral ("0.0.0.0", 0)
     */
    explicit RpcClient(uint16_t client_id,
                       uint8_t interface_version = 0x01,
                       const transport::Endpoint& local_bind = transport::Endpoint("0.0.0.0", 0));

    /**
     * @brief Borrow an exclusive, stopped transport instead of creating UDP.
     * @param client_id Unique client identifier.
     * @param transport Must outlive this client; the client manages start/stop.
     * @param interface_version Service major / Interface Version for outgoing calls.
     * @note Local binding and transport configuration belong to the supplied backend.
     * @see transport::ITransport for the injected-transport lifecycle contract.
     */
    RpcClient(uint16_t client_id, transport::ITransport& transport,
              uint8_t interface_version = 0x01);

    /**
     * @brief Destructor
     */
    ~RpcClient();

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
     * @note All pending synchronous calls are completed before application callbacks run.
     *       Explicit shutdown attempts every pending application callback; a callback that
     *       throws is caught and discarded so this function never throws (safe to call from
     *       an application RAII wrapper's destructor).
     */
    void shutdown();

    /**
     * @brief Result of the last transport start/stop (including failed-start cleanup).
     * @return SUCCESS initially, otherwise the last lifecycle outcome; not a receive error.
     * @note Safe to query during initialize/shutdown; object destruction must be serialized.
     */
    Result get_transport_result() const;

    /**
     * @brief Set the default destination for method calls
     *
     * Must be the offered service endpoint (not the SD multicast/unicast port).
     */
    void set_remote_endpoint(const transport::Endpoint& ep);

    /**
     * @brief Local endpoint after bind (port is assigned when using 0)
     */
    transport::Endpoint get_local_endpoint() const;

    /**
     * @brief Synchronous RPC method call using the configured remote endpoint
     *
     * @param service_id Target service ID
     * @param method_id Method to call
     * @param parameters Serialized method parameters
     * @param timeout Call timeout configuration
     * @return Synchronous result with return values or error
     * @note Completion and timeout removal are serialized under the pending-call mutex;
     *       an already completed response wins over timeout. No application-callback
     *       lifetime drain is performed. Join outstanding calls before destroying the client.
     *       The response-time budget starts after the request has been handed to the
     *       transport (i.e. once the, possibly blocking, send completes); the reported
     *       elapsed time reflects only the wait for a reply, not the send duration.
     */
    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const platform::ByteBuffer& parameters,
                                   const RpcTimeout& timeout = RpcTimeout());

    /**
     * @brief Synchronous RPC method call to an explicit service endpoint
     * @note Uses the same completion/timeout arbitration as the configured-endpoint overload.
     */
    RpcSyncResult call_method_sync(uint16_t service_id, MethodId method_id,
                                   const platform::ByteBuffer& parameters,
                                   const transport::Endpoint& server_endpoint,
                                   const RpcTimeout& timeout = RpcTimeout());

    /**
     * @brief Asynchronous RPC method call using the configured remote endpoint
     *
     * @param service_id Target service ID
     * @param method_id Method to call
     * @param parameters Serialized method parameters
     * @param callback Completion callback function
     * @param timeout Call timeout configuration
     * @return Call handle for cancellation, or 0 on failure
     */
    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const platform::ByteBuffer& parameters,
                                    RpcCallback callback,
                                    const RpcTimeout& timeout = RpcTimeout());

    /**
     * @brief Asynchronous RPC method call to an explicit service endpoint
     */
    RpcCallHandle call_method_async(uint16_t service_id, MethodId method_id,
                                    const platform::ByteBuffer& parameters,
                                    RpcCallback callback,
                                    const transport::Endpoint& server_endpoint,
                                    const RpcTimeout& timeout = RpcTimeout());

    /**
     * @brief Fire-and-forget REQUEST_NO_RETURN (message type 0x01)
     *
     * Does not allocate a pending-call slot or wait for a response.
     * Return Code on the wire is E_OK. Interface Version is the configured service major.
     *
     * @return true if the datagram was sent
     */
    bool send_request_no_return(uint16_t service_id, MethodId method_id,
                                const platform::ByteBuffer& params,
                                const transport::Endpoint& dest);

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
     * @brief Observe structural rejections of incoming messages.
     *
     * Additive; default is no handler. Invoked on the transport receive
     * thread. Does not send a SOME/IP error response.
     *
     * @implements REQ_TRANSPORT_026
     */
    void set_message_rejection_handler(
        platform::Function<void(const transport::MessageRejectionInfo&)> handler);

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
#ifdef SOMEIP_STATIC_ALLOC
    alignas(alignof(std::max_align_t)) char impl_storage_[SOMEIP_PIMPL_RPCCLIENT_SIZE];
    RpcClientImpl* impl() noexcept { return reinterpret_cast<RpcClientImpl*>(impl_storage_); }
    const RpcClientImpl* impl() const noexcept { return reinterpret_cast<const RpcClientImpl*>(impl_storage_); }
#else
    std::unique_ptr<RpcClientImpl> impl_;
    RpcClientImpl* impl() noexcept { return impl_.get(); }
    const RpcClientImpl* impl() const noexcept { return impl_.get(); }
#endif
};

}  // namespace someip::rpc

#endif // SOMEIP_RPC_CLIENT_H
