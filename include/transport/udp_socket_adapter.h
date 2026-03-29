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

#ifndef SOMEIP_TRANSPORT_UDP_SOCKET_ADAPTER_H
#define SOMEIP_TRANSPORT_UDP_SOCKET_ADAPTER_H

#include "transport/endpoint.h"
#include "common/result.h"
#include <functional>
#include <string>
#include <vector>

namespace someip {
namespace transport {

/**
 * @brief Callback invoked by the adapter when a datagram is received.
 *
 * Integrators call this from their I/O event path after a packet is available.
 * The payload is the raw UDP payload (one datagram).
 */
using UdpReceiveCallback = std::function<void(const std::vector<uint8_t>& data, const Endpoint& sender)>;

/**
 * @brief UDP socket abstraction for event-driven SOME/IP transport.
 *
 * Implemented by integrators using non-BSD stacks (e.g. custom datagram sockets).
 * Must not depend on platform socket headers.
 */
class IUdpSocketAdapter {
public:
    virtual ~IUdpSocketAdapter() = default;

    /**
     * @brief Open and bind the local endpoint (port 0 selects an ephemeral port).
     */
    [[nodiscard]] virtual Result open(const Endpoint& local_endpoint) = 0;

    /**
     * @brief Close the socket and release resources.
     */
    virtual void close() = 0;

    /**
     * @brief Send one datagram to the destination.
     */
    [[nodiscard]] virtual Result send(const std::vector<uint8_t>& data, const Endpoint& destination) = 0;

    /**
     * @brief Join an IPv4 multicast group.
     * @param multicast_address Group address (e.g. 224.0.0.1)
     * @param interface_address Outgoing interface address; empty uses stack default
     */
    [[nodiscard]] virtual Result join_multicast(const std::string& multicast_address,
                                                const std::string& interface_address = {}) = 0;

    /**
     * @brief Leave a multicast group previously joined.
     */
    [[nodiscard]] virtual Result leave_multicast(const std::string& multicast_address,
                                                 const std::string& interface_address = {}) = 0;

    /**
     * @brief Register the receive callback (nullptr clears).
     *
     * The adapter must invoke the callback for each received datagram from the
     * integrator's event loop or I/O thread.
     *
     * Quiescence guarantee: after set_receive_callback(nullptr) returns, the
     * adapter must not invoke any previously registered callback. Implementations
     * must ensure no in-flight callback is executing when the setter returns.
     */
    virtual void set_receive_callback(UdpReceiveCallback callback) = 0;

    /**
     * @brief Effective local endpoint after open (required after bind with port 0).
     */
    [[nodiscard]] virtual Endpoint get_local_endpoint() const = 0;
};

} // namespace transport
} // namespace someip

#endif // SOMEIP_TRANSPORT_UDP_SOCKET_ADAPTER_H
