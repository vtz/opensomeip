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

UdpTransport::UdpTransport(const Endpoint& local_endpoint)
    : local_endpoint_(local_endpoint),
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
        return Result::NETWORK_ERROR;
    }

    // Enable multicast loopback for local testing
    int loop = 1;
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
        // Not critical, continue
    }

    // Set multicast TTL (default is 1, which is usually fine)
    int ttl = 1;
    if (setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        // Not critical, continue
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
    int reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return Result::NETWORK_ERROR;
    }

    // Set non-blocking mode
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

Result UdpTransport::bind_socket() {
    std::scoped_lock lock(socket_mutex_);

    sockaddr_in addr = create_sockaddr(local_endpoint_);
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

Result UdpTransport::configure_multicast(const Endpoint& endpoint) {
    if (!is_multicast_address(endpoint.get_address())) {
        return Result::INVALID_ENDPOINT;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(endpoint.get_address().c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return Result::NETWORK_ERROR;
    }

    return Result::SUCCESS;
}

void UdpTransport::receive_loop() {
    std::vector<uint8_t> buffer(RECEIVE_BUFFER_SIZE);

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
        } else if (result == Result::NETWORK_ERROR) {
            // Network error, notify listener
            if (listener_) {
                listener_->on_error(result);
            }

            // Small delay to prevent tight error loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
    std::scoped_lock lock(socket_mutex_);

    if (socket_fd_ < 0) {
        return Result::NOT_CONNECTED;
    }

    sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t received = recvfrom(socket_fd_, data.data(), data.size(), 0,
                               reinterpret_cast<sockaddr*>(&src_addr), &addr_len);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
