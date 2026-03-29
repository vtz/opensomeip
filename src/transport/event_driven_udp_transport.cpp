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

#include "transport/event_driven_udp_transport.h"
#include "platform/memory.h"
#include <stdexcept>

namespace someip {
namespace transport {

EventDrivenUdpTransport::EventDrivenUdpTransport(IUdpSocketAdapter& adapter,
                                                 const Endpoint& local_endpoint,
                                                 const EventDrivenUdpTransportConfig& config)
    : adapter_(adapter),
      local_endpoint_(local_endpoint),
      config_(config) {
    if (!local_endpoint_.is_valid()) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw std::invalid_argument("Invalid local endpoint");
#endif
    }
}

EventDrivenUdpTransport::~EventDrivenUdpTransport() {
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    stop();
}

Result EventDrivenUdpTransport::send_message(const Message& message, const Endpoint& endpoint) {
    if (!is_running()) {
        return Result::NOT_CONNECTED;
    }
    if (!endpoint.is_valid()) {
        return Result::INVALID_ENDPOINT;
    }

    std::vector<uint8_t> data = message.serialize();
    if (data.size() > MAX_UDP_PAYLOAD) {
        return Result::BUFFER_OVERFLOW;
    }
    if (config_.max_message_size > 0 && data.size() > config_.max_message_size) {
        return Result::BUFFER_OVERFLOW;
    }

    return adapter_.send(data, endpoint);
}

MessagePtr EventDrivenUdpTransport::receive_message() {
    platform::ScopedLock lock(queue_mutex_);
    if (receive_queue_.empty()) {
        return nullptr;
    }
    MessagePtr message = receive_queue_.front();
    receive_queue_.pop();
    return message;
}

Result EventDrivenUdpTransport::connect(const Endpoint& endpoint) {
    if (!endpoint.is_valid()) {
        return Result::INVALID_ENDPOINT;
    }
    if (endpoint.get_protocol() == TransportProtocol::MULTICAST_UDP) {
        if (!is_multicast_ipv4(endpoint.get_address())) {
            return Result::INVALID_ENDPOINT;
        }
        return adapter_.join_multicast(endpoint.get_address(), config_.multicast_interface);
    }
    return Result::SUCCESS;
}

Result EventDrivenUdpTransport::disconnect() {
    return Result::SUCCESS;
}

bool EventDrivenUdpTransport::is_connected() const {
    return is_running() && opened_.load();
}

Endpoint EventDrivenUdpTransport::get_local_endpoint() const {
    if (opened_.load()) {
        return adapter_.get_local_endpoint();
    }
    return local_endpoint_;
}

void EventDrivenUdpTransport::set_listener(ITransportListener* listener) {
    listener_.store(listener, std::memory_order_release);
}

Result EventDrivenUdpTransport::start() {
    if (is_running()) {
        return Result::SUCCESS;
    }

    adapter_.set_receive_callback([this](const std::vector<uint8_t>& data, const Endpoint& sender) {
        on_adapter_receive(data, sender);
    });

    Result result = adapter_.open(local_endpoint_);
    if (result != Result::SUCCESS) {
        adapter_.set_receive_callback(nullptr);
        return result;
    }

    local_endpoint_ = adapter_.get_local_endpoint();
    opened_ = true;
    running_ = true;
    return Result::SUCCESS;
}

Result EventDrivenUdpTransport::stop() {
    if (!running_.load()) {
        return Result::SUCCESS;
    }

    running_ = false;
    adapter_.set_receive_callback(nullptr);
    adapter_.close();
    opened_ = false;

    platform::ScopedLock lock(queue_mutex_);
    while (!receive_queue_.empty()) {
        receive_queue_.pop();
    }

    return Result::SUCCESS;
}

bool EventDrivenUdpTransport::is_running() const {
    return running_.load();
}

Result EventDrivenUdpTransport::join_multicast_group(const std::string& multicast_address) {
    if (!is_multicast_ipv4(multicast_address)) {
        return Result::INVALID_ENDPOINT;
    }
    return adapter_.join_multicast(multicast_address, config_.multicast_interface);
}

Result EventDrivenUdpTransport::leave_multicast_group(const std::string& multicast_address) {
    if (!is_multicast_ipv4(multicast_address)) {
        return Result::INVALID_ENDPOINT;
    }
    return adapter_.leave_multicast(multicast_address, config_.multicast_interface);
}

void EventDrivenUdpTransport::on_adapter_receive(const std::vector<uint8_t>& data, const Endpoint& sender) {
    if (!running_.load()) {
        return;
    }

    MessagePtr message = platform::allocate_message();
    if (!message) {
        auto* cb = listener_.load(std::memory_order_acquire);
        if (cb) {
            cb->on_error(Result::OUT_OF_MEMORY);
        }
        return;
    }
    if (!message->deserialize(data)) {
        auto* cb = listener_.load(std::memory_order_acquire);
        if (cb) {
            cb->on_error(Result::INVALID_MESSAGE);
        }
        return;
    }

    {
        platform::ScopedLock lock(queue_mutex_);
        receive_queue_.push(message);
    }

    auto* cb = listener_.load(std::memory_order_acquire);
    if (cb) {
        cb->on_message_received(message, sender);
    }
}

bool EventDrivenUdpTransport::is_multicast_ipv4(const std::string& address) {
    const Endpoint ep(address, 0, TransportProtocol::MULTICAST_UDP);
    return ep.is_multicast();
}

} // namespace transport
} // namespace someip
