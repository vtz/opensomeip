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

#include "transport/tcp_transport.h"
#include "common/result.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <cstdio>

namespace someip {
namespace transport {

/**
 * @brief TCP Transport constructor
 * @implements REQ_TRANSPORT_002
 * @implements REQ_TRANSPORT_003
 * @implements REQ_TRANSPORT_005
 * @satisfies feat_req_someip_850
 * @satisfies feat_req_someip_851
 */
TcpTransport::TcpTransport(const TcpTransportConfig& config)
    : config_(config) {
}

TcpTransport::~TcpTransport() {
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall) - intentional cleanup
    stop();
}

Result TcpTransport::initialize(const Endpoint& local_endpoint) {
    // Create TCP-specific endpoint
    local_endpoint_ = Endpoint(local_endpoint.get_address(), local_endpoint.get_port(), TransportProtocol::TCP);

    // Create socket
    Result result = create_socket();
    if (result != Result::SUCCESS) {
        return result;
    }

    // Bind socket
    result = bind_socket();
    if (result != Result::SUCCESS) {
        return result;
    }

    // Update local endpoint with the actual bound port (useful when port was 0)
    sockaddr_in bound_addr;
    socklen_t addr_len = sizeof(bound_addr);
    if (getsockname(connection_.socket_fd, (sockaddr*)&bound_addr, &addr_len) == 0) {
        local_endpoint_ = Endpoint(local_endpoint_.get_address(), ntohs(bound_addr.sin_port));
    }

    return Result::SUCCESS;
}

Result TcpTransport::send_message(const Message& message, const Endpoint& endpoint) {
    if (!is_connected()) {
        return Result::NOT_CONNECTED;
    }

    // For TCP, we ignore the endpoint parameter and send over the established connection
    // The endpoint is mainly used for UDP routing

    // Serialize message
    std::vector<uint8_t> data = message.serialize();

    // Send data
    Result result = send_data(connection_.socket_fd, data);
    if (result == Result::SUCCESS) {
        connection_.update_activity();
    }

    return result;
}

MessagePtr TcpTransport::receive_message() {
    std::scoped_lock lock(queue_mutex_);
    if (message_queue_.empty()) {
        return nullptr;
    }

    auto [message, sender] = message_queue_.front();
    message_queue_.pop();
    return message;
}

Result TcpTransport::connect(const Endpoint& endpoint) {
    if (is_connected()) {
        return Result::SUCCESS;  // Already connected
    }

    if (server_mode_) {
        return Result::INVALID_STATE;  // Server mode doesn't connect
    }

    return connect_internal(endpoint);
}

Result TcpTransport::disconnect() {
    if (!is_connected()) {
        return Result::SUCCESS;  // Already disconnected
    }

    disconnect_internal();
    return Result::SUCCESS;
}

bool TcpTransport::is_connected() const {
    return connection_.is_connected();
}

Endpoint TcpTransport::get_local_endpoint() const {
    return local_endpoint_;
}

void TcpTransport::set_listener(ITransportListener* listener) {
    listener_ = listener;
}

Result TcpTransport::start() {
    if (running_) {
        return Result::SUCCESS;
    }

    running_ = true;

    // Start receive thread
    receive_thread_ = std::thread(&TcpTransport::receive_loop, this);

    // Start connection monitor thread
    connection_thread_ = std::thread(&TcpTransport::connection_monitor_loop, this);

    return Result::SUCCESS;
}

Result TcpTransport::stop() {
    if (!running_) {
        return Result::SUCCESS;
    }

    running_ = false;

    // Close connections
    disconnect_internal();

    // Close listen socket if in server mode
    if (server_mode_ && listen_socket_fd_ != -1) {
        close(listen_socket_fd_);
        listen_socket_fd_ = -1;
    }

    // Wait for threads to finish
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    if (connection_thread_.joinable()) {
        connection_thread_.join();
    }

    return Result::SUCCESS;
}

bool TcpTransport::is_running() const {
    return running_;
}

TcpConnectionState TcpTransport::get_connection_state() const {
    return connection_.state;
}

Result TcpTransport::enable_server_mode(int backlog) {
    if (connection_.socket_fd == -1) {
        return Result::NOT_INITIALIZED;
    }

    if (listen(connection_.socket_fd, backlog) < 0) {
        return Result::NETWORK_ERROR;
    }

    server_mode_ = true;
    listen_socket_fd_ = connection_.socket_fd;

    return Result::SUCCESS;
}

int TcpTransport::accept_connection() {
    if (!server_mode_ || listen_socket_fd_ == -1) {
        return -1;
    }

    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // For server mode, make the accept blocking temporarily
    int flags = fcntl(listen_socket_fd_, F_GETFL, 0);
    fcntl(listen_socket_fd_, F_SETFL, flags & ~O_NONBLOCK);

    int client_fd = accept(listen_socket_fd_, (sockaddr*)&client_addr, &client_len);

    // Restore non-blocking mode
    fcntl(listen_socket_fd_, F_SETFL, flags);

    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Accept failed
        }
        return -1;
    }

    // Set socket options for client connection (blocking for simplicity)
    setup_socket_options(client_fd, true);

    return client_fd;
}

// Private helper methods

Result TcpTransport::create_socket() {
    connection_.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection_.socket_fd < 0) {
        return Result::NETWORK_ERROR;
    }

    // Set socket options (listening socket should be non-blocking)
    return setup_socket_options(connection_.socket_fd, false);
}

Result TcpTransport::bind_socket() {
    if (connection_.socket_fd == -1) {
        return Result::NOT_INITIALIZED;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(local_endpoint_.get_port());
    addr.sin_addr.s_addr = inet_addr(local_endpoint_.get_address().c_str());

    if (bind(connection_.socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

Result TcpTransport::setup_socket_options(int socket_fd, bool blocking) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return Result::NETWORK_ERROR;
    }

    if (blocking) {
        flags &= ~O_NONBLOCK;  // Clear non-blocking flag
    } else {
        flags |= O_NONBLOCK;   // Set non-blocking flag
    }

    if (fcntl(socket_fd, F_SETFL, flags) < 0) {
        return Result::NETWORK_ERROR;
    }

    // TCP keep-alive
    if (config_.keep_alive) {
        int keep_alive = 1;
#ifdef __APPLE__
        // macOS uses different socket option levels and names
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPALIVE, &keep_alive, sizeof(keep_alive));
        int keep_alive_interval = static_cast<int>(config_.keep_alive_interval.count() / 1000);
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval));
        setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive, sizeof(keep_alive));
#else
        setsockopt(socket_fd, SOL_TCP, TCP_KEEPIDLE, &keep_alive, sizeof(keep_alive));
        int keep_alive_interval = static_cast<int>(config_.keep_alive_interval.count() / 1000);
        setsockopt(socket_fd, SOL_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval));
        setsockopt(socket_fd, SOL_TCP, TCP_KEEPCNT, &keep_alive, sizeof(keep_alive));
#endif
    }

    // Send/receive timeouts
    struct timeval send_timeout = {
        static_cast<time_t>(config_.send_timeout.count() / 1000),
        static_cast<suseconds_t>((config_.send_timeout.count() % 1000) * 1000)
    };
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

    struct timeval recv_timeout = {
        static_cast<time_t>(config_.receive_timeout.count() / 1000),
        static_cast<suseconds_t>((config_.receive_timeout.count() % 1000) * 1000)
    };
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    return Result::SUCCESS;
}

Result TcpTransport::connect_internal(const Endpoint& endpoint) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.get_port());
    addr.sin_addr.s_addr = inet_addr(endpoint.get_address().c_str());

    connection_.state = TcpConnectionState::CONNECTING;
    connection_.remote_endpoint = endpoint;

    int connect_result = ::connect(connection_.socket_fd, (sockaddr*)&addr, sizeof(addr));

    if (connect_result == 0) {
        // Connected immediately
        connection_.state = TcpConnectionState::CONNECTED;
        connection_.update_activity();

        if (listener_) {
            listener_->on_connection_established(endpoint);
        }

        return Result::SUCCESS;
    } else if (errno == EINPROGRESS) {
        // Connection in progress - wait for completion
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(connection_.socket_fd, &write_fds);

        struct timeval timeout = {
            static_cast<time_t>(config_.connection_timeout.count() / 1000),
            static_cast<suseconds_t>((config_.connection_timeout.count() % 1000) * 1000)
        };

        connect_result = select(connection_.socket_fd + 1, nullptr, &write_fds, nullptr, &timeout);

        if (connect_result > 0) {
            // Check if connection was successful
            int error = 0;
            socklen_t len = sizeof(error);
            getsockopt(connection_.socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);

            if (error == 0) {
                connection_.state = TcpConnectionState::CONNECTED;
                connection_.update_activity();

                if (listener_) {
                    listener_->on_connection_established(endpoint);
                }

                return Result::SUCCESS;
            }
        }

        // Connection failed
        disconnect_internal();
        return Result::NETWORK_ERROR;
    } else {
        // Immediate connection failure
        connection_.state = TcpConnectionState::DISCONNECTED;
        return Result::NETWORK_ERROR;
    }
}

void TcpTransport::disconnect_internal() {
    std::scoped_lock lock(connection_mutex_);

    if (connection_.socket_fd != -1) {
        connection_.state = TcpConnectionState::DISCONNECTING;

        shutdown(connection_.socket_fd, SHUT_RDWR);
        close(connection_.socket_fd);
        connection_.socket_fd = -1;

        connection_.state = TcpConnectionState::DISCONNECTED;

        // Decrement active connection count
        if (active_connections_.load() > 0) {
            active_connections_.fetch_sub(1);
        }

        if (listener_) {
            listener_->on_connection_lost(connection_.remote_endpoint);
        }
    }
}

void TcpTransport::receive_loop() {
    while (running_) {
        if (server_mode_) {
            // In server mode, accept new connections
            if (listen_socket_fd_ != -1) {
                // Check connection limit before accepting
                if (active_connections_.load() >= config_.max_connections) {
                    // Too many connections, wait a bit before checking again
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                int client_fd = accept_connection();
                if (client_fd != -1) {
                    // For this simple implementation, we'll handle one client at a time
                    // In a real implementation, you'd manage multiple client connections
                    if (!is_connected()) {
                        connection_.socket_fd = client_fd;
                        connection_.state = TcpConnectionState::CONNECTED;
                        connection_.remote_endpoint = Endpoint("127.0.0.1", 0, TransportProtocol::TCP); // Would need to get actual client address
                        active_connections_.fetch_add(1);

                        if (listener_) {
                            listener_->on_connection_established(connection_.remote_endpoint);
                        }
                    } else {
                        // Already have a connection, close this one
                        close(client_fd);
                    }
                }
            }
        }

        if (!is_connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::vector<uint8_t> buffer;
        Result result = receive_data(connection_.socket_fd, buffer);

        if (result == Result::SUCCESS && !buffer.empty()) {
            // Try to parse messages from buffer
            MessagePtr message;
            if (parse_message_from_buffer(buffer, message)) {
                std::scoped_lock lock(queue_mutex_);
                message_queue_.push({message, connection_.remote_endpoint});
                connection_.update_activity();

                if (listener_) {
                    listener_->on_message_received(message, connection_.remote_endpoint);
                }
            } else {
                // Failed to parse message from buffer
            }
        } else if (result != Result::SUCCESS) {
            // Connection error
            disconnect_internal();

            if (listener_) {
                listener_->on_error(result);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TcpTransport::connection_monitor_loop() {
    while (running_) {
        if (is_connected()) {
            auto now = std::chrono::steady_clock::now();
            auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - connection_.last_activity);

            // Check for connection timeout
            if (time_since_activity > std::chrono::minutes(5)) {
                disconnect_internal();
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

Result TcpTransport::send_data(int socket_fd, const std::vector<uint8_t>& data) {
    size_t total_sent = 0;
    const uint8_t* buffer = data.data();

    while (total_sent < data.size()) {
        ssize_t sent = send(socket_fd, buffer + total_sent, data.size() - total_sent, 0);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Retry
            }
            return Result::NETWORK_ERROR;
        } else if (sent == 0) {
            return Result::NETWORK_ERROR;  // Connection closed
        }

        total_sent += sent;
    }

    return Result::SUCCESS;
}

Result TcpTransport::receive_data(int socket_fd, std::vector<uint8_t>& data) {
    // Respect maximum receive buffer size from config
    size_t max_chunk_size = std::min(static_cast<size_t>(4096), config_.max_receive_buffer - data.size());
    if (max_chunk_size == 0) {
        return Result::BUFFER_OVERFLOW;  // Already at buffer limit
    }

    uint8_t buffer[4096];
    ssize_t received = recv(socket_fd, buffer, max_chunk_size, 0);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result::SUCCESS;  // No data available
        }
        return Result::NETWORK_ERROR;
    } else if (received == 0) {
        return Result::NETWORK_ERROR;  // Connection closed
    }

    data.insert(data.end(), buffer, buffer + received);
    return Result::SUCCESS;
}

bool TcpTransport::parse_message_from_buffer(std::vector<uint8_t>& buffer, MessagePtr& message) {
    // For TCP, we expect complete messages in the buffer since TCP is stream-oriented
    // but our current implementation receives data in chunks

    // Enforce maximum receive buffer size
    if (buffer.size() > config_.max_receive_buffer) {
        buffer.clear();  // Clear oversized buffer
        return false;
    }

    if (buffer.size() < SOMEIP_HEADER_SIZE) {
        return false;  // Need at least header
    }

    // Parse message length from header (bytes 4-7 in big-endian)
    // Length field contains length from client_id to end of message
    uint32_t length_from_client_id = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];

    if (length_from_client_id < 8 || length_from_client_id > MAX_MESSAGE_SIZE) {
        // Invalid message length - try to resync by skipping this potential header
        // Look for next potential SOME/IP header (non-zero message ID)
        size_t search_start = SOMEIP_HEADER_SIZE;
        bool found_valid_header = false;

        while (search_start + SOMEIP_HEADER_SIZE <= buffer.size()) {
            // Check if this looks like a valid SOME/IP header
            uint32_t potential_msg_id = (buffer[search_start] << 24) |
                                       (buffer[search_start + 1] << 16) |
                                       (buffer[search_start + 2] << 8) |
                                       buffer[search_start + 3];
            if (potential_msg_id != 0) {  // Found a non-zero message ID
                // Discard data before this potential header
                buffer.erase(buffer.begin(), buffer.begin() + search_start);
                found_valid_header = true;
                break;
            }
            search_start++;
        }

        if (!found_valid_header) {
            // No valid header found, clear buffer to prevent infinite loops
            buffer.clear();
        }
        return false;
    }

    // Total message size = message_id(4) + length(4) + length_from_client_id
    size_t total_message_size = 8 + length_from_client_id;

    if (buffer.size() < total_message_size) {
        return false;  // Need more data
    }

    // Extract message data
    std::vector<uint8_t> message_data(buffer.begin(), buffer.begin() + total_message_size);
    buffer.erase(buffer.begin(), buffer.begin() + total_message_size);

    // Parse message
    try {
        message = std::make_shared<Message>();
        if (message->deserialize(message_data)) {
            return true;
        }
    } catch (const std::exception& e) {
        // Message parsing exception
    } catch (...) {
        // Unknown message parsing exception
    }

    return false;
}

} // namespace transport
} // namespace someip
