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
// NOLINTNEXTLINE(misc-include-cleaner) - someip_ntohs for portable byte order
#include "platform/byteorder.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::allocate_message from memory_impl.h
#include "platform/memory.h"
// NOLINTNEXTLINE(misc-include-cleaner) - socket/POSIX types and someip_* helpers from net_impl.h
#include "platform/net.h"
#include "platform/thread.h"
#include "someip/message.h"
#include "transport/endpoint.h"
#include "transport/transport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace someip::transport {

// NOLINTBEGIN(misc-include-cleaner) - sockaddr/timeval/fd_set and someip_* wrappers/macros come from
// platform/net.h -> net_impl.h; misc-include-cleaner does not trace through this abstraction.

/**
 * @brief TCP Transport constructor
 * @implements REQ_TRANSPORT_002a, REQ_TRANSPORT_002b, REQ_TRANSPORT_003a, REQ_TRANSPORT_003b, REQ_TRANSPORT_005
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
    sockaddr_in bound_addr = {};
    socklen_t addr_len = sizeof(bound_addr);
    if (someip_getsockname(connection_.socket_fd, reinterpret_cast<struct sockaddr*>(&bound_addr), &addr_len) == 0) {
        local_endpoint_ = Endpoint(local_endpoint_.get_address(), ntohs(bound_addr.sin_port));
    }

    return Result::SUCCESS;
}

Result TcpTransport::send_message(const Message& message, const Endpoint& /*endpoint*/) {
    if (!is_connected()) {
        return Result::NOT_CONNECTED;
    }

    // Serialize message
    const std::vector<uint8_t> data = message.serialize();

    // Send data
    const Result result = send_data(connection_.socket_fd, data);
    if (result == Result::SUCCESS) {
        connection_.update_activity();
    }

    return result;
}

MessagePtr TcpTransport::receive_message() {
    platform::ScopedLock const lock(queue_mutex_);
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
    listener_.store(listener, std::memory_order_release);
}

Result TcpTransport::start() {
    if (running_) {
        return Result::SUCCESS;
    }

    running_ = true;

    // Start receive thread
    receive_thread_ = std::make_unique<platform::Thread>(&TcpTransport::receive_loop, this);

    // Start connection monitor thread
    connection_thread_ = std::make_unique<platform::Thread>(&TcpTransport::connection_monitor_loop, this);

    return Result::SUCCESS;
}

/** @implements REQ_TRANSPORT_019 */
Result TcpTransport::stop() {
    if (!running_) {
        return Result::SUCCESS;
    }

    running_.store(false, std::memory_order_release);
    listener_.store(nullptr, std::memory_order_release);

    // Close connections
    disconnect_internal();

    // Close listen socket if in server mode
    if (server_mode_ && listen_socket_fd_ != SOMEIP_INVALID_SOCKET) {
        someip_close_socket(listen_socket_fd_);
        listen_socket_fd_ = SOMEIP_INVALID_SOCKET;
    }

    // Wait for threads to finish
    if (receive_thread_ && receive_thread_->joinable()) {
        receive_thread_->join();
    }
    if (connection_thread_ && connection_thread_->joinable()) {
        connection_thread_->join();
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
    if (connection_.socket_fd == SOMEIP_INVALID_SOCKET) {
        return Result::NOT_INITIALIZED;
    }

    if (someip_listen(connection_.socket_fd, backlog) < 0) {
        return Result::NETWORK_ERROR;
    }

    server_mode_ = true;
    listen_socket_fd_ = connection_.socket_fd;

    return Result::SUCCESS;
}

/** @implements REQ_TRANSPORT_003_E01 */
someip_socket_t TcpTransport::accept_connection() {
    Endpoint unused;
    return accept_connection_with_peer(unused);
}

someip_socket_t TcpTransport::accept_connection_with_peer(Endpoint& peer_endpoint) {
    if (!server_mode_ || listen_socket_fd_ == SOMEIP_INVALID_SOCKET) {
        return SOMEIP_INVALID_SOCKET;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(listen_socket_fd_, &read_fds);

    struct timeval tv = {0, 100000}; // 100ms
    const int sel = someip_select(static_cast<int>(listen_socket_fd_) + 1, &read_fds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        return SOMEIP_INVALID_SOCKET;
    }

    sockaddr_in client_addr = {};
    socklen_t client_len = sizeof(client_addr);

    someip_socket_t const client_fd =
        someip_accept(listen_socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

    if (client_fd == SOMEIP_INVALID_SOCKET) {
        return SOMEIP_INVALID_SOCKET;
    }

    setup_socket_options(client_fd, true);

    char addr_buf[64] = {};
    someip_inet_ntop(AF_INET, &client_addr.sin_addr, addr_buf, sizeof(addr_buf));
    peer_endpoint = Endpoint(addr_buf, someip_ntohs(client_addr.sin_port), TransportProtocol::TCP);

    return client_fd;
}

// Private helper methods

Result TcpTransport::create_socket() {
    connection_.socket_fd = someip_socket(AF_INET, SOCK_STREAM, 0);
    if (connection_.socket_fd == SOMEIP_INVALID_SOCKET) {
        return Result::NETWORK_ERROR;
    }

    // Set socket options (listening socket should be non-blocking)
    return setup_socket_options(connection_.socket_fd, false);
}

Result TcpTransport::bind_socket() {
    if (connection_.socket_fd == SOMEIP_INVALID_SOCKET) {
        return Result::NOT_INITIALIZED;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(local_endpoint_.get_port());
    addr.sin_addr.s_addr = someip_inet_addr(local_endpoint_.get_address().c_str());

    if (someip_bind(connection_.socket_fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

/** @implements REQ_TRANSPORT_017 */
Result TcpTransport::setup_socket_options(someip_socket_t socket_fd, bool blocking) {
    if (blocking) {
        if (someip_set_blocking(socket_fd) < 0) {
            return Result::NETWORK_ERROR;
        }
    } else {
        if (someip_set_nonblocking(socket_fd) < 0) {
            return Result::NETWORK_ERROR;
        }
    }

    // TCP keep-alive (not available on embedded Zephyr or lwIP targets)
#if (!defined(__ZEPHYR__) || defined(CONFIG_ARCH_POSIX)) && !defined(SOMEIP_NET_LWIP)
    if (config_.keep_alive) {
        int keep_alive = 1;
        int keep_alive_interval = static_cast<int>(config_.keep_alive_interval.count() / 1000);
        someip_setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
#if defined(__APPLE__)
        someip_setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPALIVE, &keep_alive_interval, sizeof(keep_alive_interval));
#elif !defined(_WIN32)
        someip_setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_interval, sizeof(keep_alive_interval));
#endif
#if !defined(_WIN32)
        someip_setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval));
        someip_setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive, sizeof(keep_alive));
#else
        (void)keep_alive_interval;
#endif
    }
#endif

    // Send/receive timeouts
    someip_set_socket_timeout(socket_fd, SO_SNDTIMEO, static_cast<int>(config_.send_timeout.count()));
    someip_set_socket_timeout(socket_fd, SO_RCVTIMEO, static_cast<int>(config_.receive_timeout.count()));

    return Result::SUCCESS;
}

/** @implements REQ_TRANSPORT_002_E01, REQ_TRANSPORT_002_E02, REQ_TRANSPORT_002_E03, REQ_TRANSPORT_002_E04, REQ_TRANSPORT_016, REQ_TRANSPORT_016_E01, REQ_TRANSPORT_018 */
Result TcpTransport::connect_internal(const Endpoint& endpoint) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.get_port());
    addr.sin_addr.s_addr = someip_inet_addr(endpoint.get_address().c_str());

    connection_.state = TcpConnectionState::CONNECTING;
    connection_.remote_endpoint = endpoint;

    int connect_result =
        someip_connect(connection_.socket_fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));

    if (connect_result == 0) {
        connection_.state = TcpConnectionState::CONNECTED;
        connection_.receive_buffer.clear();
        last_magic_cookie_time_ = std::chrono::steady_clock::now();
        connection_.update_activity();

        if (auto* l = listener_.load(std::memory_order_acquire)) {
            l->on_connection_established(endpoint);
        }

        return Result::SUCCESS;
    } else if (someip_socket_errno() == SOMEIP_EINPROGRESS) {
        // Connection in progress - wait for completion
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(connection_.socket_fd, &write_fds);

        struct timeval timeout = {};
        timeout.tv_sec  = static_cast<decltype(timeout.tv_sec)>(config_.connection_timeout.count() / 1000);
        timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>((config_.connection_timeout.count() % 1000) * 1000);

        connect_result = someip_select(static_cast<int>(connection_.socket_fd) + 1, nullptr, &write_fds, nullptr, &timeout);

        if (connect_result > 0) {
            int error = 0;
            socklen_t len = sizeof(error);
            const int gso_ret = someip_getsockopt(connection_.socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);

            if (gso_ret == 0 && error == 0) {
                connection_.state = TcpConnectionState::CONNECTED;
                connection_.receive_buffer.clear();
                last_magic_cookie_time_ = std::chrono::steady_clock::now();
                connection_.update_activity();

                if (auto* l = listener_.load(std::memory_order_acquire)) {
                    l->on_connection_established(endpoint);
                }

                return Result::SUCCESS;
            }
        }

        disconnect_internal();
        return Result::NETWORK_ERROR;
    } else {
        // Immediate connection failure
        connection_.state = TcpConnectionState::DISCONNECTED;
        return Result::NETWORK_ERROR;
    }
}

void TcpTransport::disconnect_internal() {
    platform::ScopedLock const lock(connection_mutex_);

    if (connection_.socket_fd != SOMEIP_INVALID_SOCKET) {
        connection_.state = TcpConnectionState::DISCONNECTING;

        someip_shutdown_socket(connection_.socket_fd);
        someip_close_socket(connection_.socket_fd);
        connection_.socket_fd = SOMEIP_INVALID_SOCKET;
        connection_.receive_buffer.clear();

        connection_.state = TcpConnectionState::DISCONNECTED;

        // Decrement active connection count
        if (active_connections_.load() > 0) {
            active_connections_.fetch_sub(1);
        }

        if (auto* l = listener_.load(std::memory_order_acquire)) {
            l->on_connection_lost(connection_.remote_endpoint);
        }
    }
}

/** @implements REQ_TRANSPORT_024 */
void TcpTransport::receive_loop() {
    while (running_) {
        if (server_mode_) {
            if (listen_socket_fd_ != SOMEIP_INVALID_SOCKET) {
                if (active_connections_.load() >= config_.max_connections) {
                    platform::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                Endpoint peer_ep("0.0.0.0", 0, TransportProtocol::TCP);
                someip_socket_t const client_fd = accept_connection_with_peer(peer_ep);
                if (client_fd != SOMEIP_INVALID_SOCKET) {
                    platform::ScopedLock const lock(connection_mutex_);
                    if (!connection_.is_connected()) {
                        connection_.socket_fd = client_fd;
                        connection_.state = TcpConnectionState::CONNECTED;
                        connection_.remote_endpoint = peer_ep;
                        connection_.receive_buffer.clear();
                        last_magic_cookie_time_ = std::chrono::steady_clock::now();
                        active_connections_.fetch_add(1);

                        if (auto* l = listener_.load(std::memory_order_acquire)) {
                            l->on_connection_established(connection_.remote_endpoint);
                        }
                    } else {
                        someip_close_socket(client_fd);
                    }
                }
            }
        }

        if (!is_connected()) {
            platform::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Result result;
        {
            platform::ScopedLock const lock(connection_mutex_);
            if (connection_.socket_fd == SOMEIP_INVALID_SOCKET) {
                continue;
            }
            result = receive_data(connection_.socket_fd, connection_.receive_buffer);
        }

        if (result == Result::SUCCESS) {
            platform::ScopedLock const conn_lock(connection_mutex_);
            while (!connection_.receive_buffer.empty()) {
                MessagePtr message;
                if (!parse_message_from_buffer(connection_.receive_buffer, message)) {
                    break;
                }

                {
                    platform::ScopedLock const q_lock(queue_mutex_);
                    message_queue_.emplace(message, connection_.remote_endpoint);
                }
                connection_.update_activity();

                if (auto* l = listener_.load(std::memory_order_acquire)) {
                    l->on_message_received(message, connection_.remote_endpoint);
                }
            }
        } else if (result != Result::SUCCESS) {
            {
                platform::ScopedLock const lock(connection_mutex_);
                connection_.receive_buffer.clear();
            }
            disconnect_internal();
        }

        platform::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TcpTransport::connection_monitor_loop() {
    while (running_) {
        if (is_connected()) {
            bool timed_out = false;
            {
                platform::ScopedLock const lock(connection_mutex_);
                const auto now = std::chrono::steady_clock::now();
                const auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - connection_.last_activity);
                timed_out = (time_since_activity > std::chrono::minutes(5));
            }

            if (timed_out) {
                disconnect_internal();
            } else {
                send_periodic_magic_cookie();
            }
        }

        for (int i = 0; i < 10 && running_; ++i) {
            platform::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

/** @implements REQ_TRANSPORT_021 */
void TcpTransport::send_periodic_magic_cookie() {
    if (!config_.magic_cookie_enabled) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_magic_cookie_time_);

    if (elapsed < config_.magic_cookie_interval) {
        return;
    }

    platform::ScopedLock const lock(connection_mutex_);
    if (connection_.socket_fd == SOMEIP_INVALID_SOCKET) {
        return;
    }

    const auto cookie = server_mode_ ? make_magic_cookie_server() : make_magic_cookie_client();
    if (send_data(connection_.socket_fd, cookie) == Result::SUCCESS) {
        last_magic_cookie_time_ = now;
    }
}

/** @implements REQ_TRANSPORT_002_E01, REQ_TRANSPORT_002_E02, REQ_TRANSPORT_002_E03, REQ_TRANSPORT_002_E04 */
Result TcpTransport::send_data(someip_socket_t socket_fd, const std::vector<uint8_t>& data) {
    size_t total_sent = 0;
    const uint8_t* buffer = data.data();

    while (total_sent < data.size()) {
        ssize_t const sent = someip_send(socket_fd, buffer + total_sent,
                                         data.size() - total_sent, 0);

        if (sent < 0) {
            int const err = someip_socket_errno();
            if (err == SOMEIP_EAGAIN || err == SOMEIP_EWOULDBLOCK || err == SOMEIP_EINTR) {
                continue;
            }
            return Result::NETWORK_ERROR;
        } else if (sent == 0) {
            return Result::NETWORK_ERROR;  // Connection closed
        }

        total_sent += sent;
    }

    return Result::SUCCESS;
}

/** @implements REQ_TRANSPORT_002_E01, REQ_TRANSPORT_002_E02, REQ_TRANSPORT_002_E03, REQ_TRANSPORT_002_E04 */
Result TcpTransport::receive_data(someip_socket_t socket_fd, std::vector<uint8_t>& data) {
    // Respect maximum receive buffer size from config
    const size_t max_chunk_size = std::min(static_cast<size_t>(4096), config_.max_receive_buffer - data.size());
    if (max_chunk_size == 0) {
        return Result::BUFFER_OVERFLOW;  // Already at buffer limit
    }

    std::array<uint8_t, 4096> buffer{};
    const ssize_t received = someip_recv(socket_fd, buffer.data(), max_chunk_size, 0);

    if (received < 0) {
        int const err = someip_socket_errno();
        if (err == SOMEIP_EAGAIN || err == SOMEIP_EWOULDBLOCK || err == SOMEIP_EINTR) {
            return Result::SUCCESS;  // No data available or interrupted
        }
        return Result::NETWORK_ERROR;
    } else if (received == 0) {
        return Result::NETWORK_ERROR;  // Connection closed
    }

    data.insert(data.end(), buffer.begin(), buffer.begin() + received);
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
        return false;
    }

    if (is_magic_cookie(buffer, 0)) {
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(SOMEIP_HEADER_SIZE));
        return false;
    }

    const uint32_t length_from_client_id =
        (static_cast<uint32_t>(buffer[4]) << 24U) | (static_cast<uint32_t>(buffer[5]) << 16U) |
        (static_cast<uint32_t>(buffer[6]) << 8U) | static_cast<uint32_t>(buffer[7]);

    if (length_from_client_id < 8 || length_from_client_id > MAX_MESSAGE_SIZE) {
        size_t search_start = 1;
        bool found_valid = false;

        while (search_start + SOMEIP_HEADER_SIZE <= buffer.size()) {
            if (is_magic_cookie(buffer, search_start)) {
                buffer.erase(buffer.begin(),
                             buffer.begin() + static_cast<std::ptrdiff_t>(search_start));
                found_valid = true;
                break;
            }

            uint32_t const potential_msg_id =
                (static_cast<uint32_t>(buffer[search_start]) << 24U) |
                (static_cast<uint32_t>(buffer[search_start + 1]) << 16U) |
                (static_cast<uint32_t>(buffer[search_start + 2]) << 8U) |
                static_cast<uint32_t>(buffer[search_start + 3]);
            if (potential_msg_id != 0) {
                buffer.erase(buffer.begin(),
                             buffer.begin() + static_cast<std::ptrdiff_t>(search_start));
                found_valid = true;
                break;
            }
            search_start++;
        }

        if (!found_valid) {
            if (buffer.size() > SOMEIP_HEADER_SIZE) {
                buffer.erase(buffer.begin(),
                             buffer.begin() + static_cast<std::ptrdiff_t>(buffer.size() - SOMEIP_HEADER_SIZE + 1));
            }
        }
        return false;
    }

    // Total message size = message_id(4) + length(4) + length_from_client_id
    const size_t total_message_size = 8 + length_from_client_id;

    if (buffer.size() < total_message_size) {
        return false;  // Need more data
    }

    // Extract message data
    const auto msg_end = buffer.begin() + static_cast<std::ptrdiff_t>(total_message_size);
    const std::vector<uint8_t> message_data(buffer.begin(), msg_end);
    buffer.erase(buffer.begin(), msg_end);

    // Parse message
    message = platform::allocate_message();
    return message && message->deserialize(message_data);
}

/** @implements REQ_TRANSPORT_020, REQ_TRANSPORT_025 */
bool TcpTransport::is_magic_cookie(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + SOMEIP_HEADER_SIZE > data.size()) {
        return false;
    }
    return data[offset + 0] == 0xFF && data[offset + 1] == 0xFF &&
           (data[offset + 2] == 0x00 || data[offset + 2] == 0x80) &&
           data[offset + 3] == 0x00 &&
           data[offset + 4] == 0x00 && data[offset + 5] == 0x00 &&
           data[offset + 6] == 0x00 && data[offset + 7] == 0x08 &&
           data[offset + 8] == 0xDE && data[offset + 9] == 0xAD &&
           data[offset + 10] == 0x00 && data[offset + 11] == 0x01 &&
           data[offset + 12] == 0x01 && data[offset + 13] == 0x01 &&
           data[offset + 14] == 0xBE && data[offset + 15] == 0xEF;
}

std::vector<uint8_t> TcpTransport::make_magic_cookie_client() {
    return {
        0xFF, 0xFF, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08,
        0xDE, 0xAD, 0x00, 0x01,
        0x01, 0x01, 0xBE, 0xEF
    };
}

std::vector<uint8_t> TcpTransport::make_magic_cookie_server() {
    return {
        0xFF, 0xFF, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x08,
        0xDE, 0xAD, 0x00, 0x01,
        0x01, 0x01, 0xBE, 0xEF
    };
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::transport
