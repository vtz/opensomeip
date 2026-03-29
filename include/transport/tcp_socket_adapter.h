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

#ifndef SOMEIP_TRANSPORT_TCP_SOCKET_ADAPTER_H
#define SOMEIP_TRANSPORT_TCP_SOCKET_ADAPTER_H

#include "transport/endpoint.h"
#include "common/result.h"
#include <functional>
#include <vector>

namespace someip {
namespace transport {

/**
 * @brief Invoked when payload bytes arrive on the established connection.
 */
using TcpReceiveCallback = std::function<void(const std::vector<uint8_t>& data)>;

/**
 * @brief Invoked when the connection is ready (outgoing connect or accepted peer).
 */
using TcpConnectedCallback = std::function<void(const Endpoint& remote)>;

/**
 * @brief Invoked when the connection is closed or reset.
 */
using TcpDisconnectedCallback = std::function<void()>;

/**
 * @brief TCP socket abstraction for event-driven SOME/IP transport.
 *
 * Implemented by integrators using non-BSD stacks. The adapter owns the
 * semantics of connect/accept; it must invoke callbacks from its event context.
 */
class ITcpSocketAdapter {
public:
    virtual ~ITcpSocketAdapter() = default;

    /**
     * @brief Create socket bound to the local endpoint (listening or pre-connect).
     */
    [[nodiscard]] virtual Result open(const Endpoint& local_endpoint) = 0;

    virtual void close() = 0;

    /**
     * @brief Start listening after open (server mode). No-op or NOT_IMPLEMENTED for client-only adapters.
     */
    [[nodiscard]] virtual Result listen(int backlog) = 0;

    /**
     * @brief Connect to a remote endpoint (client mode).
     */
    [[nodiscard]] virtual Result connect(const Endpoint& remote_endpoint) = 0;

    /**
     * @brief Accept one pending connection if available (non-blocking from SOME/IP's perspective).
     * @param remote_out Peer endpoint when Result::SUCCESS
     */
    [[nodiscard]] virtual Result accept(Endpoint& remote_out) = 0;

    /**
     * @brief Send bytes on the active connection.
     */
    [[nodiscard]] virtual Result send(const std::vector<uint8_t>& data) = 0;

    /**
     * Quiescence guarantee: after any set_*_callback(nullptr) returns, the
     * adapter must not invoke that previously registered callback.
     * Implementations must ensure no in-flight callback is executing when
     * the setter returns.
     */
    virtual void set_receive_callback(TcpReceiveCallback callback) = 0;
    virtual void set_connected_callback(TcpConnectedCallback callback) = 0;
    virtual void set_disconnected_callback(TcpDisconnectedCallback callback) = 0;

    [[nodiscard]] virtual Endpoint get_local_endpoint() const = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
};

} // namespace transport
} // namespace someip

#endif // SOMEIP_TRANSPORT_TCP_SOCKET_ADAPTER_H
