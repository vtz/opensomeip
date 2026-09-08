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

#ifndef SOMEIP_TRANSPORT_TRANSPORT_H
#define SOMEIP_TRANSPORT_TRANSPORT_H

#include "transport/endpoint.h"
#include "someip/message.h"
#include "common/result.h"
#include <memory>
#include <functional>

namespace someip::transport {

/**
 * @brief Transport listener interface
 *
 * Classes implementing this interface can receive transport events
 * and incoming messages.
 */
class ITransportListener {
public:
    virtual ~ITransportListener() = default;

    ITransportListener(const ITransportListener&) = delete;
    ITransportListener& operator=(const ITransportListener&) = delete;
    ITransportListener(ITransportListener&&) = delete;
    ITransportListener& operator=(ITransportListener&&) = delete;

    /**
     * @brief Called when a message is received
     * @param message The received message
     * @param sender The sender endpoint (for responses)
     */
    virtual void on_message_received(MessagePtr message, const Endpoint& sender) = 0;

    /**
     * @brief Called when connection to endpoint is lost
     * @param endpoint The endpoint that lost connection
     */
    virtual void on_connection_lost(const Endpoint& endpoint) = 0;

    /**
     * @brief Called when connection to endpoint is established
     * @param endpoint The endpoint that established connection
     */
    virtual void on_connection_established(const Endpoint& endpoint) = 0;

    /**
     * @brief Called when a transport error occurs
     * @param error The error that occurred
     */
    virtual void on_error(Result error) = 0;

protected:
    ITransportListener() = default;
};

/**
 * @brief Transport interface
 *
 * This interface defines the contract for all transport implementations
 * (UDP, TCP, etc.) in the SOME/IP stack.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    ITransport(const ITransport&) = delete;
    ITransport& operator=(const ITransport&) = delete;
    ITransport(ITransport&&) = delete;
    ITransport& operator=(ITransport&&) = delete;

    /**
     * @brief Send a message to an endpoint
     * @param message The message to send
     * @param endpoint The destination endpoint
     * @return Result of the operation
     */
    [[nodiscard]] virtual Result send_message(const Message& message, const Endpoint& endpoint) = 0;

    /**
     * @brief Receive a message from the internal queue (non-blocking, polling mode)
     *
     * Only returns messages when no listener is installed via set_listener().
     * In listener mode, new incoming messages are dispatched through
     * ITransportListener::on_message_received and are not enqueued.
     * Messages that were already queued before a listener was installed
     * remain drainable via this method until the queue is empty.
     *
     * @return Received message or nullptr if no message available
     * @see set_listener()
     */
    virtual MessagePtr receive_message() = 0;

    /**
     * @brief Connect to an endpoint (TCP only)
     * @param endpoint The endpoint to connect to
     * @return Result of the operation
     */
    [[nodiscard]] virtual Result connect(const Endpoint& endpoint) = 0;

    /**
     * @brief Disconnect from endpoint (TCP only)
     * @return Result of the operation
     */
    [[nodiscard]] virtual Result disconnect() = 0;

    /**
     * @brief Check if connected
     * @return true if connected, false otherwise
     */
    virtual bool is_connected() const = 0;

    /**
     * @brief Get local endpoint
     * @return Local endpoint information
     */
    virtual Endpoint get_local_endpoint() const = 0;

    /**
     * @brief Set transport listener for asynchronous message delivery
     *
     * Listener mode and polling mode are mutually exclusive:
     * - When a non-null listener is set, incoming messages are dispatched
     *   exclusively through ITransportListener::on_message_received.
     *   They are **not** enqueued, so receive_message() returns nullptr.
     * - When listener is nullptr (or never set), incoming messages are
     *   enqueued for retrieval via receive_message().
     * - Messages already queued before a listener is installed remain
     *   drainable via receive_message(); only new arrivals go to the
     *   listener.
     * - Clearing the listener (passing nullptr) restores polling mode
     *   for subsequent messages.
     *
     * @note The transport stores a non-owning raw pointer. The caller
     *       must keep the listener alive until either set_listener(nullptr)
     *       is called and all in-flight on_message_received() callbacks
     *       have returned, or the transport is stopped and destroyed.
     *
     * @param listener The listener to receive events, or nullptr to
     *                 restore polling mode
     * @see receive_message()
     */
    virtual void set_listener(ITransportListener* listener) = 0;

    /**
     * @brief Start the transport (begin receiving)
     * @return Result of the operation
     */
    [[nodiscard]] virtual Result start() = 0;

    /**
     * @brief Stop the transport
     * @return Result of the operation
     */
    [[nodiscard]] virtual Result stop() = 0;

    /**
     * @brief Check if transport is running
     * @return true if running, false otherwise
     */
    virtual bool is_running() const = 0;

protected:
    ITransport() = default;
};

// Type aliases for convenience
using ITransportPtr = std::shared_ptr<ITransport>;
using ITransportListenerPtr = std::shared_ptr<ITransportListener>;

}  // namespace someip::transport

#endif // SOMEIP_TRANSPORT_TRANSPORT_H
