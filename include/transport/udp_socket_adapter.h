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

#include "common/result.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "transport/endpoint.h"

namespace someip::transport {

/**
 * @brief Callback invoked by the adapter when a datagram is received.
 *
 * Integrators call this from their I/O event path after a packet is available.
 * The payload is the raw UDP payload (one datagram).
 */
using UdpReceiveCallback =
    platform::Function<void(const platform::ByteBuffer& data, const Endpoint& sender)>;

/**
 * @brief UDP socket abstraction for event-driven SOME/IP transport.
 *
 * Implemented by integrators using non-BSD stacks (e.g. custom datagram sockets).
 * Must not depend on platform socket headers.
 */
class IUdpSocketAdapter {
   public:
    virtual ~IUdpSocketAdapter() = default;

    IUdpSocketAdapter(const IUdpSocketAdapter&) = delete;
    IUdpSocketAdapter& operator=(const IUdpSocketAdapter&) = delete;
    IUdpSocketAdapter(IUdpSocketAdapter&&) = delete;
    IUdpSocketAdapter& operator=(IUdpSocketAdapter&&) = delete;

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
    [[nodiscard]] virtual Result send(const platform::ByteBuffer& data,
                                      const Endpoint& destination) = 0;

    /**
     * @brief Join an IPv4 multicast group.
     * @param multicast_address Group address (e.g. 224.0.0.1)
     * @param interface_address Outgoing interface address; empty uses stack default
     */
    [[nodiscard]] virtual Result join_multicast(
        const platform::String<>& multicast_address,
        const platform::String<>& interface_address = {}) = 0;

    /**
     * @brief Leave a multicast group previously joined.
     */
    [[nodiscard]] virtual Result leave_multicast(
        const platform::String<>& multicast_address,
        const platform::String<>& interface_address = {}) = 0;

    /**
     * @brief Register the receive callback (nullptr clears).
     *
     * The adapter must invoke the callback for each received datagram from the
     * integrator's event loop or I/O thread.
     *
     * Callback ordering: connected/receive callbacks for a given socket must not
     * run concurrently with each other. Deliver connected before the first receive
     * that depends on the peer endpoint.
     *
     * Quiescence: after set_receive_callback(nullptr) returns to an *external*
     * caller, the adapter must not invoke any previously registered callback, and
     * no in-flight callback from another thread may still be executing. When
     * clear is invoked *from inside* the active receive callback (e.g. listener
     * calls stop()), the setter must return without waiting for that same
     * callback to finish — otherwise a single-threaded adapter would deadlock.
     */
    virtual void set_receive_callback(UdpReceiveCallback callback) = 0;

    /**
     * @brief Effective local endpoint after open (required after bind with port 0).
     */
    [[nodiscard]] virtual Endpoint get_local_endpoint() const = 0;

   protected:
    IUdpSocketAdapter() = default;
};

}  // namespace someip::transport

#endif  // SOMEIP_TRANSPORT_UDP_SOCKET_ADAPTER_H
