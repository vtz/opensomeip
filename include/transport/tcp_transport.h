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

#ifndef SOMEIP_TRANSPORT_TCP_TRANSPORT_H
#define SOMEIP_TRANSPORT_TCP_TRANSPORT_H

#include "transport/transport.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/net.h"
#include "platform/thread.h"
#include <atomic>
#include <cstddef>
#include <optional>

/**
 * @brief Compile-time upper bound on concurrently served TCP connections.
 *
 * Static builds size the connection table at compile time, so this bound also
 * caps TcpTransportConfig::max_connections. Each served connection may hold a
 * receive buffer drawn from the byte pool, so raising this on a static build
 * usually means raising SOMEIP_BYTE_POOL_* counts as well.
 */
#ifndef SOMEIP_MAX_TCP_CONNECTIONS
#define SOMEIP_MAX_TCP_CONNECTIONS 8
#endif

namespace someip::transport {

/**
 * @brief TCP Connection State
 */
enum class TcpConnectionState : uint8_t {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

/**
 * @brief TCP Connection Information
 */
struct TcpConnection {
    someip_socket_t socket_fd{SOMEIP_INVALID_SOCKET};
    Endpoint remote_endpoint;
    TcpConnectionState state{TcpConnectionState::DISCONNECTED};
    std::chrono::steady_clock::time_point last_activity{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point last_magic_cookie{std::chrono::steady_clock::now()};
    platform::ByteBuffer receive_buffer;

    TcpConnection() = default;

    bool is_connected() const {
        return state == TcpConnectionState::CONNECTED;
    }

    void update_activity() {
        last_activity = std::chrono::steady_clock::now();
    }
};

/**
 * @brief TCP Transport Configuration
 */
struct TcpTransportConfig {
    std::chrono::milliseconds connection_timeout{5000};     // Connection timeout
    std::chrono::milliseconds receive_timeout{100};        // Receive timeout
    std::chrono::milliseconds send_timeout{1000};          // Send timeout
    size_t max_receive_buffer{65536};                       // Max receive buffer size
    size_t max_connections{8};                              // Max concurrent connections (clamped to SOMEIP_MAX_TCP_CONNECTIONS)
    bool keep_alive{true};                                  // TCP keep-alive
    std::chrono::milliseconds keep_alive_interval{30000};   // Keep-alive interval
    bool magic_cookie_enabled{true};                        // Periodic Magic Cookie insertion
    std::chrono::milliseconds magic_cookie_interval{10000}; // 10s per SOME/IP spec
};

/**
 * @brief TCP Transport Implementation
 *
 * Provides reliable, connection-oriented transport for SOME/IP messages
 * using TCP sockets. Supports both client and server modes.
 */
class TcpTransport : public ITransport {
public:
    /**
     * @brief Constructor
     * @param config TCP transport configuration
     */
    explicit TcpTransport(const TcpTransportConfig& config = TcpTransportConfig());

    /**
     * @brief Destructor
     */
    ~TcpTransport() override;

    // Delete copy and move operations
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;
    TcpTransport(TcpTransport&&) = delete;
    TcpTransport& operator=(TcpTransport&&) = delete;

    /**
     * @brief Initialize the transport
     * @param local_endpoint Local endpoint to bind to
     * @return Result of the operation
     */
    [[nodiscard]] Result initialize(const Endpoint& local_endpoint);

    /**
     * @brief Send a message
     * @param message The message to send
     * @param endpoint The destination endpoint
     * @return Result of the operation
     */
    [[nodiscard]] Result send_message(const Message& message, const Endpoint& endpoint) override;

    /**
     * @brief Receive a message from the internal queue (non-blocking, polling mode)
     *
     * Only returns messages when no listener is installed via set_listener().
     * Messages queued before listener installation remain drainable.
     *
     * @return Received message or nullptr if no message available
     * @see set_listener()
     */
    MessagePtr receive_message() override;

    /**
     * @brief Connect to a remote endpoint
     * @param endpoint The endpoint to connect to
     * @return Result of the operation
     */
    Result connect(const Endpoint& endpoint) override;

    /**
     * @brief Disconnect from current connection
     * @return Result of the operation
     */
    Result disconnect() override;

    /**
     * @brief Check if connected to remote endpoint
     * @return true if connected, false otherwise
     */
    bool is_connected() const override;

    /**
     * @brief Get local endpoint
     * @return Local endpoint information
     */
    Endpoint get_local_endpoint() const override;

    /**
     * @brief Set transport listener for asynchronous message delivery
     *
     * @copydetails ITransport::set_listener()
     */
    void set_listener(ITransportListener* listener) override;

    /**
     * @brief Start the transport
     * @return Result of the operation
     */
    Result start() override;

    /**
     * @brief Stop the transport
     * @return Result of the operation
     */
    Result stop() override;

    /**
     * @brief Check if transport is running
     * @return true if running, false otherwise
     */
    bool is_running() const override;

    /**
     * @brief Get current connection state
     *
     * A server may serve several peers at once. This reports CONNECTED while at
     * least one peer is connected; use connection_count() or
     * is_peer_connected() to inspect individual peers.
     *
     * @return Connection state
     */
    TcpConnectionState get_connection_state() const;

    /**
     * @brief Number of peers currently connected
     * @return Connection count (0..max_connections())
     */
    size_t connection_count() const;

    /**
     * @brief Effective concurrent connection limit
     *
     * TcpTransportConfig::max_connections clamped to SOMEIP_MAX_TCP_CONNECTIONS.
     *
     * @return Maximum number of peers served concurrently
     */
    size_t max_connections() const;

    /**
     * @brief Check whether a specific peer is connected
     * @param peer Remote endpoint, matched on address and port
     * @return true if a connection to that peer is established
     */
    bool is_peer_connected(const Endpoint& peer) const;

    /**
     * @brief Close the connection to one peer, leaving others untouched
     * @param peer Remote endpoint, matched on address and port
     * @return SUCCESS if the peer was connected and is now closed,
     *         NOT_CONNECTED otherwise
     */
    Result disconnect_peer(const Endpoint& peer);

    /**
     * @brief Enable server mode (listen for incoming connections)
     * @param backlog Maximum number of pending connections
     * @return Result of the operation
     */
    Result enable_server_mode(int backlog = 5);

    /**
     * @brief Accept incoming connection (server mode)
     * @return New connection socket FD or -1 on error
     */
    someip_socket_t accept_connection();

    /**
     * @brief Parse one complete SOME/IP message from a byte buffer.
     *
     * Consumes exactly the bytes of one message if successful; leaves
     * incomplete trailing bytes in the buffer for subsequent calls.
     *
     * @param buffer Accumulation buffer (modified in-place)
     * @param message [out] Parsed message on success
     * @return true if a complete message was extracted
     */
    bool parse_message_from_buffer(platform::ByteBuffer& buffer, MessagePtr& message);

    static constexpr size_t SOMEIP_HEADER_SIZE = 16;
    static constexpr size_t MAX_MESSAGE_SIZE = 65535;

    /** @implements REQ_TRANSPORT_020, REQ_TRANSPORT_025 */
    static bool is_magic_cookie(const platform::ByteBuffer& data, size_t offset = 0);
    static platform::ByteBuffer make_magic_cookie_client();
    static platform::ByteBuffer make_magic_cookie_server();

private:
    /// Established peer connections. A client holds at most one entry.
    using ConnectionTable = platform::Vector<TcpConnection, SOMEIP_MAX_TCP_CONNECTIONS>;
    /// Peer endpoints batched for notification outside connection_mutex_.
    using EndpointList = platform::Vector<Endpoint, SOMEIP_MAX_TCP_CONNECTIONS>;

    TcpTransportConfig config_;
    Endpoint local_endpoint_;
    std::atomic<ITransportListener*> listener_{nullptr};

    std::atomic<bool> running_{false};
    std::optional<platform::Thread> receive_thread_;
    std::optional<platform::Thread> connection_thread_;

    platform::Queue<std::pair<MessagePtr, Endpoint>> message_queue_;
    platform::Mutex queue_mutex_;
    platform::ConditionVariable queue_cv_;

    /// Guards connections_ and bound_socket_fd_. Listener callbacks are always
    /// invoked with this released, so a listener may re-enter the transport.
    mutable platform::Mutex connection_mutex_;
    ConnectionTable connections_;
    bool server_mode_{false};

    /// Socket created by initialize(). Ownership moves to listen_socket_fd_ on
    /// enable_server_mode(), or into connections_ on a successful connect().
    someip_socket_t bound_socket_fd_{SOMEIP_INVALID_SOCKET};
    someip_socket_t listen_socket_fd_{SOMEIP_INVALID_SOCKET};

    void deliver_or_enqueue(const MessagePtr& message, const Endpoint& sender);
    someip_socket_t accept_connection_with_peer(Endpoint& peer_endpoint);
    Result create_socket();
    Result bind_socket();
    Result setup_socket_options(someip_socket_t socket_fd, bool blocking = true);
    Result connect_internal(const Endpoint& endpoint);
    void disconnect_internal();
    void receive_loop();
    void connection_monitor_loop();
    void send_periodic_magic_cookie();
    Result send_data(someip_socket_t socket_fd, const platform::ByteBuffer& data);
    Result receive_data(someip_socket_t socket_fd, platform::ByteBuffer& data);

    // Connection table helpers. The _locked suffix requires connection_mutex_.
    TcpConnection* find_peer_locked(const Endpoint& peer);
    TcpConnection* find_socket_locked(someip_socket_t socket_fd);
    void close_peer_locked(size_t index);
    void close_socket_and_notify(someip_socket_t socket_fd);
    void notify_peers_lost(const EndpointList& peers);

    /// Accept one pending peer (server mode), if below the connection limit.
    void accept_pending_peer();
    /// Read from one peer socket and dispatch every complete message on it.
    void service_peer(someip_socket_t socket_fd);
};

}  // namespace someip::transport

#endif // SOMEIP_TRANSPORT_TCP_TRANSPORT_H
