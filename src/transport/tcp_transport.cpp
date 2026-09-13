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
#include <utility>

namespace someip::transport {

// NOLINTBEGIN(misc-include-cleaner) - sockaddr/timeval/fd_set and someip_* wrappers/macros come from
// platform/net.h -> net_impl.h; misc-include-cleaner does not trace through this abstraction.

namespace {

/// Endpoint::operator== also compares the protocol, but get_local_endpoint()
/// reports no protocol, so peers are identified by address and port alone.
bool same_peer(const Endpoint& lhs, const Endpoint& rhs) {
    return lhs.get_port() == rhs.get_port() && lhs.get_address() == rhs.get_address();
}

}  // namespace

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

    // stop() returns early when the transport was never started, so anything
    // opened by initialize()/connect()/enable_server_mode() is released here.
    listener_.store(nullptr, std::memory_order_release);
    disconnect_internal();

    platform::ScopedLock const lock(table_mutex_);
    if (listen_socket_fd_ != SOMEIP_INVALID_SOCKET) {
        someip_close_socket(listen_socket_fd_);
        listen_socket_fd_ = SOMEIP_INVALID_SOCKET;
    }
    if (bound_socket_fd_ != SOMEIP_INVALID_SOCKET) {
        someip_close_socket(bound_socket_fd_);
        bound_socket_fd_ = SOMEIP_INVALID_SOCKET;
    }
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
    if (someip_getsockname(bound_socket_fd_,
                           reinterpret_cast<struct sockaddr*>(&bound_addr), &addr_len) == 0) {
        local_endpoint_ = Endpoint(local_endpoint_.get_address(), ntohs(bound_addr.sin_port));
    }

    return Result::SUCCESS;
}

Result TcpTransport::send_message(const Message& message, const Endpoint& endpoint) {
    if (!endpoint.is_valid()) {
        return Result::INVALID_ENDPOINT;
    }

    // Serialize message
    const platform::ByteBuffer data = message.serialize();

    // I/O rights on the peer's slot keep its descriptor alive for the duration
    // of the send without holding the slot table, so a peer that is slow to
    // drain does not stall traffic to any other peer.
    someip_socket_t socket_fd = SOMEIP_INVALID_SOCKET;
    ConnectionSlot* const slot = acquire_io(endpoint, socket_fd);
    if (slot == nullptr) {
        return Result::NOT_CONNECTED;
    }

    const Result result = send_data(socket_fd, data);

    if (result == Result::SUCCESS) {
        platform::ScopedLock const lock(table_mutex_);
        slot->conn.update_activity();
    }
    release_io(*slot);

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

    bool server_mode = false;
    {
        platform::ScopedLock const lock(table_mutex_);
        server_mode = server_mode_;
    }
    if (server_mode) {
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
    platform::ScopedLock const lock(table_mutex_);
    for (const auto& slot : slots_) {
        if (slot.state == SlotState::ACTIVE && slot.conn.is_connected()) {
            return true;
        }
    }
    return false;
}

size_t TcpTransport::connection_count() const {
    platform::ScopedLock const lock(table_mutex_);
    return active_count_locked();
}

/** @implements REQ_TRANSPORT_003_E01 */
size_t TcpTransport::max_connections() const {
    const size_t configured = (config_.max_connections == 0U) ? 1U : config_.max_connections;
    return std::min(configured, MAX_TCP_CONNECTIONS);
}

bool TcpTransport::is_peer_connected(const Endpoint& peer) const {
    platform::ScopedLock const lock(table_mutex_);
    for (const auto& slot : slots_) {
        if (slot.state == SlotState::ACTIVE && slot.conn.is_connected() &&
            same_peer(slot.conn.remote_endpoint, peer)) {
            return true;
        }
    }
    return false;
}

Result TcpTransport::disconnect_peer(const Endpoint& peer) {
    EndpointList lost;
    ConnectionSlot* slot = nullptr;
    {
        platform::ScopedLock const lock(table_mutex_);
        slot = find_active_peer_locked(peer);
        if (slot == nullptr) {
            return Result::NOT_CONNECTED;
        }
        lost.push_back(slot->conn.remote_endpoint);
    }

    close_slot(*slot);
    notify_peers_lost(lost);
    return Result::SUCCESS;
}

TcpTransport::ConnectionSlot* TcpTransport::find_active_peer_locked(const Endpoint& peer) {
    for (auto& slot : slots_) {
        if (slot.state == SlotState::ACTIVE && slot.conn.is_connected() &&
            same_peer(slot.conn.remote_endpoint, peer)) {
            return &slot;
        }
    }
    return nullptr;
}

TcpTransport::ConnectionSlot* TcpTransport::find_active_socket_locked(someip_socket_t socket_fd) {
    if (socket_fd == SOMEIP_INVALID_SOCKET) {
        return nullptr;
    }
    for (auto& slot : slots_) {
        if (slot.state == SlotState::ACTIVE && slot.conn.socket_fd == socket_fd) {
            return &slot;
        }
    }
    return nullptr;
}

TcpTransport::ConnectionSlot* TcpTransport::allocate_slot_locked() {
    if (active_count_locked() >= max_connections()) {
        return nullptr;
    }
    for (auto& slot : slots_) {
        if (slot.state == SlotState::FREE) {
            return &slot;
        }
    }
    return nullptr;
}

size_t TcpTransport::active_count_locked() const {
    size_t count = 0;
    for (const auto& slot : slots_) {
        if (slot.state == SlotState::ACTIVE) {
            ++count;
        }
    }
    return count;
}

TcpTransport::ConnectionSlot* TcpTransport::acquire_io(const Endpoint& peer,
                                                       someip_socket_t& fd_out) {
    ConnectionSlot* slot = nullptr;
    {
        platform::ScopedLock const lock(table_mutex_);
        slot = find_active_peer_locked(peer);
        if (slot == nullptr) {
            return nullptr;
        }
    }

    // Waited on with table_mutex_ released, so a slow peer blocks only other
    // traffic to itself rather than the whole table.
    slot->io_mutex.lock();

    platform::ScopedLock const lock(table_mutex_);
    if (slot->state != SlotState::ACTIVE || !slot->conn.is_connected() ||
        !same_peer(slot->conn.remote_endpoint, peer)) {
        slot->io_mutex.unlock();  // Closed and possibly recycled while we waited.
        return nullptr;
    }
    fd_out = slot->conn.socket_fd;
    return slot;
}

TcpTransport::ConnectionSlot* TcpTransport::acquire_io(someip_socket_t socket_fd) {
    ConnectionSlot* slot = nullptr;
    {
        platform::ScopedLock const lock(table_mutex_);
        slot = find_active_socket_locked(socket_fd);
        if (slot == nullptr) {
            return nullptr;
        }
    }

    slot->io_mutex.lock();

    platform::ScopedLock const lock(table_mutex_);
    if (slot->state != SlotState::ACTIVE || slot->conn.socket_fd != socket_fd) {
        slot->io_mutex.unlock();
        return nullptr;
    }
    return slot;
}

void TcpTransport::release_io(ConnectionSlot& slot) {
    slot.io_mutex.unlock();
}

void TcpTransport::close_slot(ConnectionSlot& slot) {
    someip_socket_t fd = SOMEIP_INVALID_SOCKET;
    {
        platform::ScopedLock const lock(table_mutex_);
        if (slot.state != SlotState::ACTIVE) {
            return;  // Another thread is already tearing this slot down.
        }
        // CLOSING keeps the slot off-limits to new I/O and stops it being
        // handed to a new peer before the descriptor is actually released.
        slot.state = SlotState::CLOSING;
        slot.conn.state = TcpConnectionState::DISCONNECTING;
        fd = slot.conn.socket_fd;
    }

    // Waits out any I/O already in flight on this peer, without holding the
    // table. The descriptor stays valid for that thread until it finishes.
    slot.io_mutex.lock();
    if (fd != SOMEIP_INVALID_SOCKET) {
        someip_shutdown_socket(fd);
        someip_close_socket(fd);
    }
    {
        platform::ScopedLock const lock(table_mutex_);
        slot.conn.socket_fd = SOMEIP_INVALID_SOCKET;
        slot.conn.state = TcpConnectionState::DISCONNECTED;
        slot.conn.receive_buffer.clear();
        slot.state = SlotState::FREE;
    }
    slot.io_mutex.unlock();
}

void TcpTransport::close_socket_and_notify(someip_socket_t socket_fd) {
    EndpointList lost;
    ConnectionSlot* slot = nullptr;
    {
        platform::ScopedLock const lock(table_mutex_);
        slot = find_active_socket_locked(socket_fd);
        if (slot == nullptr) {
            return;
        }
        lost.push_back(slot->conn.remote_endpoint);
    }

    close_slot(*slot);
    notify_peers_lost(lost);
}

void TcpTransport::notify_peers_lost(const EndpointList& peers) {
    auto* l = listener_.load(std::memory_order_acquire);
    if (l == nullptr) {
        return;
    }
    for (const auto& peer : peers) {
        l->on_connection_lost(peer);
    }
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
    receive_thread_.emplace(&TcpTransport::receive_loop, this);

    // Start connection monitor thread
    connection_thread_.emplace(&TcpTransport::connection_monitor_loop, this);

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

    {
        platform::ScopedLock const lock(table_mutex_);

        // The listen socket is owned separately from every peer connection, so
        // closing it here cannot double-close a descriptor already released above.
        if (listen_socket_fd_ != SOMEIP_INVALID_SOCKET) {
            someip_close_socket(listen_socket_fd_);
            listen_socket_fd_ = SOMEIP_INVALID_SOCKET;
        }

        // Bound but never promoted to a listener or a connection.
        if (bound_socket_fd_ != SOMEIP_INVALID_SOCKET) {
            someip_close_socket(bound_socket_fd_);
            bound_socket_fd_ = SOMEIP_INVALID_SOCKET;
        }
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
    platform::ScopedLock const lock(table_mutex_);
    for (const auto& slot : slots_) {
        if (slot.state == SlotState::ACTIVE && slot.conn.is_connected()) {
            return TcpConnectionState::CONNECTED;
        }
    }
    return TcpConnectionState::DISCONNECTED;
}

Result TcpTransport::enable_server_mode(int backlog) {
    platform::ScopedLock const lock(table_mutex_);

    if (bound_socket_fd_ == SOMEIP_INVALID_SOCKET) {
        return Result::NOT_INITIALIZED;
    }

    if (someip_listen(bound_socket_fd_, backlog) < 0) {
        return Result::NETWORK_ERROR;
    }

    // Ownership moves to the listen socket, which is never part of the slot
    // table and is closed only by stop().
    listen_socket_fd_ = bound_socket_fd_;
    bound_socket_fd_ = SOMEIP_INVALID_SOCKET;
    server_mode_ = true;

    return Result::SUCCESS;
}

someip_socket_t TcpTransport::accept_connection() {
    Endpoint unused;
    return accept_connection_with_peer(unused);
}

someip_socket_t TcpTransport::accept_connection_with_peer(Endpoint& peer_endpoint) {
    // Snapshot the descriptor: stop() may invalidate the member concurrently,
    // and FD_SET/FD_ISSET on -1 trips the glibc FD_* fortify checks.
    someip_socket_t listen_fd = SOMEIP_INVALID_SOCKET;
    {
        platform::ScopedLock const lock(table_mutex_);
        if (!server_mode_) {
            return SOMEIP_INVALID_SOCKET;
        }
        listen_fd = listen_socket_fd_;
    }

    if (listen_fd == SOMEIP_INVALID_SOCKET) {
        return SOMEIP_INVALID_SOCKET;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds);

    struct timeval tv = {0, 100000}; // 100ms
    const int sel =
        someip_select(static_cast<int>(listen_fd) + 1, &read_fds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        return SOMEIP_INVALID_SOCKET;
    }

    sockaddr_in client_addr = {};
    socklen_t client_len = sizeof(client_addr);

    someip_socket_t const client_fd =
        someip_accept(listen_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

    if (client_fd == SOMEIP_INVALID_SOCKET) {
        return SOMEIP_INVALID_SOCKET;
    }

    setup_socket_options(client_fd, true);

    std::array<char, 64> addr_buf = {};
    someip_inet_ntop(AF_INET, &client_addr.sin_addr, addr_buf.data(), addr_buf.size());
    peer_endpoint = Endpoint(addr_buf.data(), someip_ntohs(client_addr.sin_port), TransportProtocol::TCP);

    return client_fd;
}

// Private helper methods

Result TcpTransport::create_socket() {
    bound_socket_fd_ = someip_socket(AF_INET, SOCK_STREAM, 0);
    if (bound_socket_fd_ == SOMEIP_INVALID_SOCKET) {
        return Result::NETWORK_ERROR;
    }

    // Set socket options (listening socket should be non-blocking)
    return setup_socket_options(bound_socket_fd_, false);
}

Result TcpTransport::bind_socket() {
    if (bound_socket_fd_ == SOMEIP_INVALID_SOCKET) {
        return Result::NOT_INITIALIZED;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(local_endpoint_.get_port());
    addr.sin_addr.s_addr = someip_inet_addr(local_endpoint_.get_address().c_str());

    if (someip_bind(bound_socket_fd_,
                    reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0) {
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
    someip_socket_t socket_fd = SOMEIP_INVALID_SOCKET;
    {
        platform::ScopedLock const lock(table_mutex_);
        socket_fd = bound_socket_fd_;
    }

    if (socket_fd == SOMEIP_INVALID_SOCKET) {
        return Result::NOT_INITIALIZED;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.get_port());
    addr.sin_addr.s_addr = someip_inet_addr(endpoint.get_address().c_str());

    int connect_result =
        someip_connect(socket_fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));

    if (connect_result != 0 && someip_socket_errno() == SOMEIP_EINPROGRESS) {
        // Connection in progress - wait for completion
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd, &write_fds);

        struct timeval timeout = {};
        timeout.tv_sec  = static_cast<decltype(timeout.tv_sec)>(config_.connection_timeout.count() / 1000);
        timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>((config_.connection_timeout.count() % 1000) * 1000);

        connect_result = -1;
        if (someip_select(static_cast<int>(socket_fd) + 1, nullptr, &write_fds,
                          nullptr, &timeout) > 0) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (someip_getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 &&
                error == 0) {
                connect_result = 0;
            }
        }
    }

    if (connect_result != 0) {
        // The bound socket may have a pending error and cannot be reused.
        platform::ScopedLock const lock(table_mutex_);
        someip_close_socket(bound_socket_fd_);
        bound_socket_fd_ = SOMEIP_INVALID_SOCKET;
        return Result::NETWORK_ERROR;
    }

    {
        platform::ScopedLock const lock(table_mutex_);
        ConnectionSlot* const slot = allocate_slot_locked();
        if (slot == nullptr) {
            someip_close_socket(bound_socket_fd_);
            bound_socket_fd_ = SOMEIP_INVALID_SOCKET;
            return Result::RESOURCE_EXHAUSTED;
        }

        slot->conn.socket_fd = socket_fd;
        slot->conn.remote_endpoint = endpoint;
        slot->conn.state = TcpConnectionState::CONNECTED;
        slot->conn.receive_buffer.clear();
        slot->conn.update_activity();
        slot->conn.last_magic_cookie = std::chrono::steady_clock::now();
        slot->state = SlotState::ACTIVE;

        // Ownership now belongs to the slot.
        bound_socket_fd_ = SOMEIP_INVALID_SOCKET;
    }

    if (auto* l = listener_.load(std::memory_order_acquire)) {
        l->on_connection_established(endpoint);
    }

    return Result::SUCCESS;
}

void TcpTransport::disconnect_internal() {
    EndpointList lost;
    {
        platform::ScopedLock const lock(table_mutex_);
        for (const auto& slot : slots_) {
            if (slot.state == SlotState::ACTIVE) {
                lost.push_back(slot.conn.remote_endpoint);
            }
        }
    }

    // close_slot() takes table_mutex_ itself and waits on each slot's io_mutex,
    // so it must be called with the table released.
    for (auto& slot : slots_) {
        close_slot(slot);
    }

    notify_peers_lost(lost);
}

/** @implements REQ_TRANSPORT_024 */
void TcpTransport::deliver_or_enqueue(const MessagePtr& message, const Endpoint& sender) {
    auto* l = listener_.load(std::memory_order_acquire);
    if (l != nullptr) {
        l->on_message_received(message, sender);
    } else {
        platform::ScopedLock const q_lock(queue_mutex_);
        message_queue_.emplace(message, sender);
    }
}

/** @implements REQ_TRANSPORT_003a, REQ_TRANSPORT_003b, REQ_TRANSPORT_003_E01 */
void TcpTransport::accept_pending_peer() {
    Endpoint peer_ep("0.0.0.0", 0, TransportProtocol::TCP);
    someip_socket_t const client_fd = accept_connection_with_peer(peer_ep);
    if (client_fd == SOMEIP_INVALID_SOCKET) {
        return;
    }

    bool accepted = false;
    {
        platform::ScopedLock const lock(table_mutex_);
        ConnectionSlot* const slot = allocate_slot_locked();
        if (slot != nullptr) {
            slot->conn.socket_fd = client_fd;
            slot->conn.remote_endpoint = peer_ep;
            slot->conn.state = TcpConnectionState::CONNECTED;
            slot->conn.receive_buffer.clear();
            slot->conn.update_activity();
            slot->conn.last_magic_cookie = std::chrono::steady_clock::now();
            slot->state = SlotState::ACTIVE;
            accepted = true;
        }
    }

    if (!accepted) {
        // At the connection limit: refuse this peer without disturbing the
        // established ones. Shutting down before closing makes the refusal
        // observable to the client instead of leaving it half-open.
        someip_shutdown_socket(client_fd);
        someip_close_socket(client_fd);
        return;
    }

    if (auto* l = listener_.load(std::memory_order_acquire)) {
        l->on_connection_established(peer_ep);
    }
}

void TcpTransport::service_peer(someip_socket_t socket_fd) {
    ConnectionSlot* const slot = acquire_io(socket_fd);
    if (slot == nullptr) {
        return;
    }

    // The recv() runs with only this peer's io_mutex held, so a peer that
    // dribbles bytes cannot hold up the slot table. The bytes land in a local
    // buffer and are appended to the connection under table_mutex_ afterwards.
    size_t room = 0;
    {
        platform::ScopedLock const lock(table_mutex_);
        const size_t buffered = slot->conn.receive_buffer.size();
        room = (buffered >= config_.max_receive_buffer) ? 0U
                                                        : config_.max_receive_buffer - buffered;
    }

    std::array<uint8_t, 4096> chunk{};
    size_t received = 0;
    Result result = receive_data(socket_fd, chunk, room, received);

    if (result == Result::SUCCESS && received > 0) {
        platform::ScopedLock const lock(table_mutex_);
        platform::ByteBuffer& buffer = slot->conn.receive_buffer;
        const size_t size_before = buffer.size();
        buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + received);
        // On static builds the buffer is pool-backed and growth can fail
        // silently, which would drop bytes mid-stream and desynchronise framing.
        if (buffer.size() != size_before + received) {
            result = Result::BUFFER_OVERFLOW;
        } else {
            slot->conn.update_activity();
        }
    }

    if (result != Result::SUCCESS) {
        platform::ScopedLock const lock(table_mutex_);
        slot->conn.receive_buffer.clear();
    }

    release_io(*slot);

    if (result != Result::SUCCESS) {
        close_socket_and_notify(socket_fd);
        return;
    }

    // Deliver every complete message with table_mutex_ released, so a
    // listener may call back into the transport.
    while (running_) {
        MessagePtr message;
        Endpoint sender_ep;
        {
            platform::ScopedLock const lock(table_mutex_);
            if (slot->state != SlotState::ACTIVE || slot->conn.socket_fd != socket_fd ||
                slot->conn.receive_buffer.empty()) {
                break;
            }
            if (!parse_message_from_buffer(slot->conn.receive_buffer, message)) {
                break;
            }
            slot->conn.update_activity();
            sender_ep = slot->conn.remote_endpoint;
        }
        deliver_or_enqueue(message, sender_ep);
    }
}

/** @implements REQ_TRANSPORT_003a, REQ_TRANSPORT_003b */
void TcpTransport::receive_loop() {
    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = -1;

        // Copied out of the member so stop() cannot invalidate it between
        // select() and FD_ISSET(), which would trip the FD_* fortify checks.
        someip_socket_t listen_fd = SOMEIP_INVALID_SOCKET;

        // Snapshot the descriptors so select() runs without table_mutex_.
        {
            platform::ScopedLock const lock(table_mutex_);

            // Watched even at capacity. accept_pending_peer() then accepts and
            // immediately closes the surplus peer, so the client observes a
            // prompt refusal rather than completing a handshake into the kernel
            // backlog and waiting on a server that will never serve it.
            if (listen_socket_fd_ != SOMEIP_INVALID_SOCKET) {
                listen_fd = listen_socket_fd_;
                FD_SET(listen_fd, &read_fds);
                max_fd = std::max(max_fd, static_cast<int>(listen_fd));
            }

            for (const auto& slot : slots_) {
                if (slot.state == SlotState::ACTIVE &&
                    slot.conn.socket_fd != SOMEIP_INVALID_SOCKET) {
                    FD_SET(slot.conn.socket_fd, &read_fds);
                    max_fd = std::max(max_fd, static_cast<int>(slot.conn.socket_fd));
                }
            }
        }

        if (max_fd < 0) {
            // Nothing to watch yet: no listener and no connections.
            platform::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        struct timeval tv = {0, 100000}; // 100ms
        if (someip_select(max_fd + 1, &read_fds, nullptr, nullptr, &tv) <= 0) {
            continue;
        }

        if (listen_fd != SOMEIP_INVALID_SOCKET && FD_ISSET(listen_fd, &read_fds)) {
            accept_pending_peer();
        }

        // Collect readable peers first; service_peer() re-locks per peer.
        platform::Vector<someip_socket_t, MAX_TCP_CONNECTIONS> readable;
        {
            platform::ScopedLock const lock(table_mutex_);
            for (const auto& slot : slots_) {
                if (slot.state == SlotState::ACTIVE &&
                    slot.conn.socket_fd != SOMEIP_INVALID_SOCKET &&
                    FD_ISSET(slot.conn.socket_fd, &read_fds)) {
                    readable.push_back(slot.conn.socket_fd);
                }
            }
        }

        for (const someip_socket_t socket_fd : readable) {
            if (!running_) {
                break;
            }
            service_peer(socket_fd);
        }
    }
}

void TcpTransport::connection_monitor_loop() {
    while (running_) {
        EndpointList timed_out;
        platform::Vector<ConnectionSlot*, MAX_TCP_CONNECTIONS> expired;
        {
            platform::ScopedLock const lock(table_mutex_);
            const auto now = std::chrono::steady_clock::now();

            for (auto& slot : slots_) {
                if (slot.state != SlotState::ACTIVE) {
                    continue;
                }
                const auto idle = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - slot.conn.last_activity);
                if (idle > std::chrono::minutes(5)) {
                    timed_out.push_back(slot.conn.remote_endpoint);
                    expired.push_back(&slot);
                }
            }
        }

        // close_slot() re-takes table_mutex_ and waits on each slot's io_mutex.
        for (ConnectionSlot* const slot : expired) {
            close_slot(*slot);
        }

        notify_peers_lost(timed_out);
        send_periodic_magic_cookie();

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

    // Pick the due peers under the table lock, then send with it released:
    // cookies go to every peer in turn, and one unresponsive peer must not hold
    // the table for the whole round.
    platform::Vector<Endpoint, MAX_TCP_CONNECTIONS> due;
    platform::ByteBuffer cookie;
    {
        platform::ScopedLock const lock(table_mutex_);
        cookie = server_mode_ ? make_magic_cookie_server() : make_magic_cookie_client();
        for (const auto& slot : slots_) {
            if (slot.state != SlotState::ACTIVE ||
                slot.conn.socket_fd == SOMEIP_INVALID_SOCKET) {
                continue;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - slot.conn.last_magic_cookie);
            if (elapsed >= config_.magic_cookie_interval) {
                due.push_back(slot.conn.remote_endpoint);
            }
        }
    }

    for (const auto& peer : due) {
        someip_socket_t socket_fd = SOMEIP_INVALID_SOCKET;
        ConnectionSlot* const slot = acquire_io(peer, socket_fd);
        if (slot == nullptr) {
            continue;  // Peer went away between selection and sending.
        }
        const Result result = send_data(socket_fd, cookie);
        if (result == Result::SUCCESS) {
            platform::ScopedLock const lock(table_mutex_);
            slot->conn.last_magic_cookie = now;
        }
        release_io(*slot);
    }
}

/** @implements REQ_TRANSPORT_002_E01, REQ_TRANSPORT_002_E02, REQ_TRANSPORT_002_E03, REQ_TRANSPORT_002_E04 */
Result TcpTransport::send_data(someip_socket_t socket_fd, const platform::ByteBuffer& data) {
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
Result TcpTransport::receive_data(someip_socket_t socket_fd, std::array<uint8_t, 4096>& buffer,
                                  size_t max_chunk, size_t& received) {
    received = 0;

    const size_t max_chunk_size = std::min(buffer.size(), max_chunk);
    if (max_chunk_size == 0) {
        return Result::BUFFER_OVERFLOW;  // Already at buffer limit
    }

    const ssize_t bytes = someip_recv(socket_fd, buffer.data(), max_chunk_size, 0);

    if (bytes < 0) {
        int const err = someip_socket_errno();
        if (err == SOMEIP_EAGAIN || err == SOMEIP_EWOULDBLOCK || err == SOMEIP_EINTR) {
            return Result::SUCCESS;  // No data available or interrupted
        }
        return Result::NETWORK_ERROR;
    }
    if (bytes == 0) {
        return Result::NETWORK_ERROR;  // Connection closed
    }

    received = static_cast<size_t>(bytes);
    return Result::SUCCESS;
}

bool TcpTransport::parse_message_from_buffer(platform::ByteBuffer& buffer, MessagePtr& message) {
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
    const platform::ByteBuffer message_data(buffer.begin(), msg_end);
    buffer.erase(buffer.begin(), msg_end);

    // Parse message
    message = platform::allocate_message();
    return message && message->deserialize(message_data);
}

/** @implements REQ_TRANSPORT_020, REQ_TRANSPORT_025 */
bool TcpTransport::is_magic_cookie(const platform::ByteBuffer& data, size_t offset) {
    if (offset + SOMEIP_HEADER_SIZE > data.size()) {
        return false;
    }
    // Common fields: Service 0xFFFF, Length 8, Client 0xDEAD, Session 0xBEEF,
    // Proto 1, Iface 1, RetCode 0.
    // Method ID and Message Type must correlate:
    //   Client cookie: Method 0x0000, MsgType 0x01 (REQUEST)
    //   Server cookie: Method 0x8000, MsgType 0x02 (NOTIFICATION)
    const bool common =
        data[offset + 0] == 0xFF && data[offset + 1] == 0xFF &&
        data[offset + 3] == 0x00 &&
        data[offset + 4] == 0x00 && data[offset + 5] == 0x00 &&
        data[offset + 6] == 0x00 && data[offset + 7] == 0x08 &&
        data[offset + 8] == 0xDE && data[offset + 9] == 0xAD &&
        data[offset + 10] == 0xBE && data[offset + 11] == 0xEF &&
        data[offset + 12] == 0x01 && data[offset + 13] == 0x01 &&
        data[offset + 15] == 0x00;
    if (!common) {
        return false;
    }
    const bool is_client = data[offset + 2] == 0x00 && data[offset + 14] == 0x01;
    const bool is_server = data[offset + 2] == 0x80 && data[offset + 14] == 0x02;
    return is_client || is_server;
}

platform::ByteBuffer TcpTransport::make_magic_cookie_client() {
    return {
        0xFF, 0xFF, 0x00, 0x00,  // Service 0xFFFF, Method 0x0000
        0x00, 0x00, 0x00, 0x08,  // Length 8
        0xDE, 0xAD, 0xBE, 0xEF,  // Client 0xDEAD, Session 0xBEEF
        0x01, 0x01, 0x01, 0x00   // Proto 1, Iface 1, MsgType 0x01, RetCode 0x00
    };
}

platform::ByteBuffer TcpTransport::make_magic_cookie_server() {
    return {
        0xFF, 0xFF, 0x80, 0x00,  // Service 0xFFFF, Method 0x8000
        0x00, 0x00, 0x00, 0x08,  // Length 8
        0xDE, 0xAD, 0xBE, 0xEF,  // Client 0xDEAD, Session 0xBEEF
        0x01, 0x01, 0x02, 0x00   // Proto 1, Iface 1, MsgType 0x02, RetCode 0x00
    };
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::transport
