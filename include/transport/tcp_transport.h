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
#include "platform/thread.h"
#include <cstddef>
#include <atomic>
#include <queue>

namespace someip {
namespace transport {

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
    int socket_fd{-1};
    Endpoint remote_endpoint;
    TcpConnectionState state{TcpConnectionState::DISCONNECTED};
    std::chrono::steady_clock::time_point last_activity{std::chrono::steady_clock::now()};
    std::vector<uint8_t> receive_buffer;

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
    size_t max_connections{10};                             // Max concurrent connections
    bool keep_alive{true};                                  // TCP keep-alive
    std::chrono::milliseconds keep_alive_interval{30000};   // Keep-alive interval
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
     * @brief Receive a message (non-blocking)
     * @return Received message or nullptr if no message available
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
     * @brief Set transport listener
     * @param listener The listener to receive events
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
     * @return Connection state
     */
    TcpConnectionState get_connection_state() const;

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
    int accept_connection();

private:
    TcpTransportConfig config_;
    Endpoint local_endpoint_;
    TcpConnection connection_;
    ITransportListener* listener_{nullptr};

    std::atomic<bool> running_{false};
    std::unique_ptr<platform::Thread> receive_thread_;
    std::unique_ptr<platform::Thread> connection_thread_;

    std::atomic<size_t> active_connections_{0};

    std::queue<std::pair<MessagePtr, Endpoint>> message_queue_;
    platform::Mutex queue_mutex_;
    platform::ConditionVariable queue_cv_;

    platform::Mutex connection_mutex_;
    bool server_mode_{false};
    int listen_socket_fd_{-1};

    // Helper methods
    Result create_socket();
    Result bind_socket();
    Result setup_socket_options(int socket_fd, bool blocking = true);
    Result connect_internal(const Endpoint& endpoint);
    void disconnect_internal();
    void receive_loop();
    void connection_monitor_loop();
    Result send_data(int socket_fd, const std::vector<uint8_t>& data);
    Result receive_data(int socket_fd, std::vector<uint8_t>& data);
    bool parse_message_from_buffer(std::vector<uint8_t>& buffer, MessagePtr& message);

    // Message parsing
    static const size_t SOMEIP_HEADER_SIZE = 16;
    static const size_t MAX_MESSAGE_SIZE = 65535;
};

} // namespace transport
} // namespace someip

#endif // SOMEIP_TRANSPORT_TCP_TRANSPORT_H
