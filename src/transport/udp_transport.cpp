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

#include "transport/udp_transport.h"
#include "common/result.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace someip {
namespace transport {

UdpTransport::UdpTransport(const Endpoint& local_endpoint, const UdpTransportConfig& config)
    : local_endpoint_(local_endpoint),
      config_(config),
      running_(false) {
    if (!local_endpoint_.is_valid()) {
        throw std::invalid_argument("Invalid local endpoint");
    }
}

UdpTransport::~UdpTransport() {
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall) - intentional cleanup
    stop();
}

Result UdpTransport::send_message(const Message& message, const Endpoint& endpoint) {
    if (!is_running()) {
        return Result::NOT_CONNECTED;
    }

    if (!endpoint.is_valid()) {
        return Result::INVALID_ENDPOINT;
    }

    // Serialize message
    std::vector<uint8_t> data = message.serialize();

    if (data.size() > MAX_UDP_PAYLOAD) {
        return Result::BUFFER_OVERFLOW;
    }

    // Check against SOME/IP recommended max size (1400 bytes to avoid IP fragmentation)
    if (config_.max_message_size > 0 && data.size() > config_.max_message_size) {
        // Log warning but allow sending - use TP for large messages
        // In production, this should trigger SOME/IP-TP segmentation
    }

    return send_data(data, endpoint);
}

MessagePtr UdpTransport::receive_message() {
    std::scoped_lock lock(queue_mutex_);
    if (receive_queue_.empty()) {
        return nullptr;
    }

    MessagePtr message = receive_queue_.front();
    receive_queue_.pop();
    return message;
}

Result UdpTransport::connect(const Endpoint& endpoint) {
    // UDP is connectionless, so this just validates the endpoint
    if (!endpoint.is_valid()) {
        return Result::INVALID_ENDPOINT;
    }

    // For multicast, join the group
    if (endpoint.get_protocol() == TransportProtocol::MULTICAST_UDP) {
        return configure_multicast(endpoint);
    }

    return Result::SUCCESS;
}

Result UdpTransport::disconnect() {
    // UDP is connectionless, nothing to disconnect
    return Result::SUCCESS;
}

bool UdpTransport::is_connected() const {
    return is_running() && socket_fd_ >= 0;
}

Endpoint UdpTransport::get_local_endpoint() const {
    return local_endpoint_;
}

void UdpTransport::set_listener(ITransportListener* listener) {
    listener_ = listener;
}

Result UdpTransport::start() {
    if (is_running()) {
        return Result::SUCCESS;
    }

    Result result = create_socket();
    if (result != Result::SUCCESS) {
        return result;
    }

    result = bind_socket();
    if (result != Result::SUCCESS) {
        close(socket_fd_);
        socket_fd_ = -1;
        return result;
    }

    running_ = true;
    receive_thread_ = std::thread(&UdpTransport::receive_loop, this);

    return Result::SUCCESS;
}

Result UdpTransport::stop() {
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall) - safe: no override expected
    if (!running_.load()) {
        return Result::SUCCESS;
    }

    running_ = false;

    // Close socket to wake up receive thread
    if (socket_fd_ >= 0) {
        // Shutdown first to wake up any blocking calls
        shutdown(socket_fd_, SHUT_RDWR);
        close(socket_fd_);
        socket_fd_ = -1;
    }

    // Wait for receive thread to finish
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    return Result::SUCCESS;
}

bool UdpTransport::is_running() const {
    return running_;
}

Result UdpTransport::join_multicast_group(const std::string& multicast_address) {
    std::scoped_lock lock(socket_mutex_);

    if (socket_fd_ < 0) {
        return Result::NOT_CONNECTED;
    }

    if (!is_multicast_address(multicast_address)) {
        return Result::INVALID_ENDPOINT;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_address.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        // In containerized/CI environments, multicast may not be available
        // Continue without multicast support rather than failing
        // This allows SOME/IP to work with unicast-only networking
    }

    // Enable multicast loopback for local testing
    int loop = 1;
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
        // Not critical, continue
    }

    // Set multicast TTL from config (per SOME/IP spec, default 1 = local network only)
    int ttl = config_.multicast_ttl;
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        // Not critical, continue
    }

    // Set multicast interface if specified
    if (!config_.multicast_interface.empty()) {
        struct in_addr interface_addr;
        interface_addr.s_addr = inet_addr(config_.multicast_interface.c_str());
        if (setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_IF, &interface_addr, sizeof(interface_addr)) < 0) {
            // Not critical, continue
        }
    }

    return Result::SUCCESS;
}

Result UdpTransport::leave_multicast_group(const std::string& multicast_address) {
    std::scoped_lock lock(socket_mutex_);

    if (socket_fd_ < 0) {
        return Result::NOT_CONNECTED;
    }

    if (!is_multicast_address(multicast_address)) {
        return Result::INVALID_ENDPOINT;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_address.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket_fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

Result UdpTransport::create_socket() {
    std::scoped_lock lock(socket_mutex_);

    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return Result::NETWORK_ERROR;
    }

    // Set socket options
    if (config_.reuse_address) {
        int reuse = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return Result::NETWORK_ERROR;
        }
    }

    // SO_REUSEPORT allows multiple processes to bind to the same port
    // Required for multicast SD when multiple applications share port 30490
#ifdef SO_REUSEPORT
    if (config_.reuse_port) {
        int reuse = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
            // Not critical - some systems don't support SO_REUSEPORT
        }
    }
#endif

    if (config_.enable_broadcast) {
        int broadcast = 1;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return Result::NETWORK_ERROR;
        }
    }

    // Set buffer sizes (non-critical - may fail in restricted environments like CI/containers)
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &config_.receive_buffer_size, sizeof(config_.receive_buffer_size)) < 0) {
        // Not critical - continue with default buffer size
    }

    if (setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &config_.send_buffer_size, sizeof(config_.send_buffer_size)) < 0) {
        // Not critical - continue with default buffer size
    }

    // Set blocking/non-blocking mode
    if (!config_.blocking) {
        int flags = fcntl(socket_fd_, F_GETFL, 0);
        if (flags < 0 || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return Result::NETWORK_ERROR;
        }
    }

    return Result::SUCCESS;
}

Result UdpTransport::bind_socket() {
    std::scoped_lock lock(socket_mutex_);

    sockaddr_in addr = create_sockaddr(local_endpoint_);
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return Result::NETWORK_ERROR;
    }

    // Get the actual port assigned by the OS (important for port 0)
    socklen_t addr_len = sizeof(addr);
    if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        local_endpoint_ = sockaddr_to_endpoint(addr);
    }

    return Result::SUCCESS;
}

Result UdpTransport::configure_multicast(const Endpoint& endpoint) {
    if (!is_multicast_address(endpoint.get_address())) {
        return Result::INVALID_ENDPOINT;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(endpoint.get_address().c_str());

    // Use configured interface or INADDR_ANY
    if (!config_.multicast_interface.empty()) {
        mreq.imr_interface.s_addr = inet_addr(config_.multicast_interface.c_str());
    } else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }

    if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

void UdpTransport::receive_loop() {
    std::vector<uint8_t> buffer(config_.receive_buffer_size);

    while (running_) {
        Endpoint sender;
        Result result = receive_data(buffer, sender);

        if (result == Result::SUCCESS) {
            // Try to deserialize message
            MessagePtr message = std::make_shared<Message>();
            if (message->deserialize(buffer)) {  // Deserialize from the received buffer
                // Add to queue
                {
                    std::scoped_lock lock(queue_mutex_);
                    receive_queue_.push(message);
                }
                queue_cv_.notify_one();

                // Notify listener with sender information
                if (listener_) {
                    listener_->on_message_received(message, sender);
                }
            }
        } else if (result == Result::NOT_CONNECTED) {
            // Socket was closed, exit loop
            break;
        } else if (result == Result::TIMEOUT && !config_.blocking) {
            // Timeout in non-blocking mode - just continue polling
            // Small delay to prevent tight polling loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            // Network or other error, notify listener
            if (listener_) {
                listener_->on_error(result);
            }

            if (!config_.blocking) {
                // In non-blocking mode, add delay to prevent busy loops on errors
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            // In blocking mode, we only get here on actual errors, no delay needed
        }
    }
}

Result UdpTransport::send_data(const std::vector<uint8_t>& data, const Endpoint& endpoint) {
    std::scoped_lock lock(socket_mutex_);

    if (socket_fd_ < 0) {
        return Result::NOT_CONNECTED;
    }

    sockaddr_in dest_addr = create_sockaddr(endpoint);
    ssize_t sent = sendto(socket_fd_, data.data(), data.size(), 0,
                         reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));

    if (sent < 0) {
        return Result::NETWORK_ERROR;
    }

    if (static_cast<size_t>(sent) != data.size()) {
        return Result::BUFFER_OVERFLOW;
    }

    return Result::SUCCESS;
}

Result UdpTransport::receive_data(std::vector<uint8_t>& data, Endpoint& sender) {
    sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t received = recvfrom(socket_fd_, data.data(), data.size(), 0,
                               reinterpret_cast<sockaddr*>(&src_addr), &addr_len);

    if (received < 0) {
        // Socket was closed during shutdown
        if (errno == EBADF || errno == EINTR) {
            return Result::NOT_CONNECTED;
        }

        // In non-blocking mode, EAGAIN/EWOULDBLOCK means no data available
        if (!config_.blocking && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return Result::TIMEOUT;
        }

        return Result::NETWORK_ERROR;
    }

    sender = sockaddr_to_endpoint(src_addr);
    data.resize(received);

    return Result::SUCCESS;
}

sockaddr_in UdpTransport::create_sockaddr(const Endpoint& endpoint) const {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.get_port());
    addr.sin_addr.s_addr = inet_addr(endpoint.get_address().c_str());
    return addr;
}

Endpoint UdpTransport::sockaddr_to_endpoint(const sockaddr_in& addr) const {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));

    return Endpoint(ip_str, ntohs(addr.sin_port), TransportProtocol::UDP);
}

bool UdpTransport::is_multicast_address(const std::string& address) const {
    in_addr_t addr = inet_addr(address.c_str());
    if (addr == INADDR_NONE) {
        return false;
    }

    // Check if address is in multicast range (224.0.0.0 - 239.255.255.255)
    uint32_t host_addr = ntohl(addr);
    return (host_addr >= 0xE0000000) && (host_addr <= 0xEFFFFFFF);
}

} // namespace transport
} // namespace someip
