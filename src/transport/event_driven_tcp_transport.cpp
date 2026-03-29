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

#include "transport/event_driven_tcp_transport.h"
#include "platform/memory.h"
#include <algorithm>
#include <cstddef>

namespace someip {
namespace transport {

const size_t EventDrivenTcpTransport::SOMEIP_HEADER_SIZE = 16;
const size_t EventDrivenTcpTransport::MAX_MESSAGE_SIZE = 65535;

EventDrivenTcpTransport::EventDrivenTcpTransport(ITcpSocketAdapter& adapter,
                                                   const EventDrivenTcpTransportConfig& config)
    : adapter_(adapter),
      config_(config) {}

EventDrivenTcpTransport::~EventDrivenTcpTransport() {
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    stop();
}

Result EventDrivenTcpTransport::initialize(const Endpoint& local_endpoint) {
    if (initialized_.load()) {
        return Result::INVALID_STATE;
    }

    local_endpoint_ =
        Endpoint(local_endpoint.get_address(), local_endpoint.get_port(), TransportProtocol::TCP);

    Result result = adapter_.open(local_endpoint_);
    if (result != Result::SUCCESS) {
        return result;
    }

    local_endpoint_ = adapter_.get_local_endpoint();
    initialized_ = true;
    return Result::SUCCESS;
}

Result EventDrivenTcpTransport::enable_server_mode(int backlog) {
    if (!initialized_.load()) {
        return Result::NOT_INITIALIZED;
    }
    Result result = adapter_.listen(backlog);
    if (result != Result::SUCCESS) {
        return result;
    }
    server_mode_ = true;
    return Result::SUCCESS;
}

Result EventDrivenTcpTransport::try_accept_connection(Endpoint& remote_out) {
    if (!server_mode_) {
        return Result::INVALID_STATE;
    }
    if (!running_.load()) {
        return Result::INVALID_STATE;
    }
    return adapter_.accept(remote_out);
}

Result EventDrivenTcpTransport::send_message(const Message& message, const Endpoint& /*endpoint*/) {
    if (!is_connected()) {
        return Result::NOT_CONNECTED;
    }

    std::vector<uint8_t> data = message.serialize();
    return adapter_.send(data);
}

MessagePtr EventDrivenTcpTransport::receive_message() {
    platform::ScopedLock lock(queue_mutex_);
    if (message_queue_.empty()) {
        return nullptr;
    }
    MessagePtr message = message_queue_.front().first;
    message_queue_.pop();
    return message;
}

Result EventDrivenTcpTransport::connect(const Endpoint& endpoint) {
    if (server_mode_) {
        return Result::INVALID_STATE;
    }
    if (is_connected()) {
        return Result::SUCCESS;
    }
    if (!initialized_.load()) {
        return Result::NOT_INITIALIZED;
    }
    if (!running_.load()) {
        return Result::INVALID_STATE;
    }
    return adapter_.connect(endpoint);
}

Result EventDrivenTcpTransport::disconnect() {
    if (!adapter_.is_connected() && !initialized_.load()) {
        return Result::SUCCESS;
    }
    adapter_.close();
    {
        platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.clear();
    }
    initialized_ = false;
    return Result::SUCCESS;
}

bool EventDrivenTcpTransport::is_connected() const {
    return adapter_.is_connected();
}

Endpoint EventDrivenTcpTransport::get_local_endpoint() const {
    if (initialized_.load()) {
        return adapter_.get_local_endpoint();
    }
    return local_endpoint_;
}

void EventDrivenTcpTransport::set_listener(ITransportListener* listener) {
    listener_.store(listener, std::memory_order_release);
}

Result EventDrivenTcpTransport::start() {
    if (!initialized_.load()) {
        return Result::NOT_INITIALIZED;
    }
    if (running_.load()) {
        return Result::SUCCESS;
    }

    adapter_.set_receive_callback([this](const std::vector<uint8_t>& data) { on_adapter_receive(data); });
    adapter_.set_connected_callback([this](const Endpoint& remote) { on_adapter_connected(remote); });
    adapter_.set_disconnected_callback([this]() { on_adapter_disconnected(); });

    running_ = true;
    return Result::SUCCESS;
}

Result EventDrivenTcpTransport::stop() {
    running_ = false;

    adapter_.set_receive_callback(nullptr);
    adapter_.set_connected_callback(nullptr);
    adapter_.set_disconnected_callback(nullptr);

    adapter_.close();
    initialized_ = false;
    server_mode_ = false;

    {
        platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.clear();
        while (!message_queue_.empty()) {
            message_queue_.pop();
        }
    }

    return Result::SUCCESS;
}

bool EventDrivenTcpTransport::is_running() const {
    return running_.load();
}

void EventDrivenTcpTransport::on_adapter_receive(const std::vector<uint8_t>& data) {
    if (!running_.load() || !initialized_.load()) {
        return;
    }

    std::vector<MessagePtr> delivered;
    {
        platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.insert(receive_buffer_.end(), data.begin(), data.end());
        MessagePtr message;
        while (parse_message_from_buffer(receive_buffer_, message)) {
            message_queue_.push({message, connection_remote_});
            delivered.push_back(message);
        }
    }

    auto* cb = listener_.load(std::memory_order_acquire);
    for (const MessagePtr& m : delivered) {
        if (cb) {
            cb->on_message_received(m, connection_remote_);
        }
    }
}

void EventDrivenTcpTransport::on_adapter_connected(const Endpoint& remote) {
    if (!running_.load()) {
        return;
    }
    connection_remote_ = remote;
    auto* cb = listener_.load(std::memory_order_acquire);
    if (cb) {
        cb->on_connection_established(remote);
    }
}

void EventDrivenTcpTransport::on_adapter_disconnected() {
    if (!running_.load()) {
        return;
    }
    Endpoint lost = connection_remote_;
    {
        platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.clear();
    }
    auto* cb = listener_.load(std::memory_order_acquire);
    if (cb) {
        cb->on_connection_lost(lost);
    }
}

bool EventDrivenTcpTransport::parse_message_from_buffer(std::vector<uint8_t>& buffer, MessagePtr& message) {
    for (;;) {
        if (buffer.size() > config_.max_receive_buffer) {
            buffer.clear();
            return false;
        }

        if (buffer.size() < SOMEIP_HEADER_SIZE) {
            return false;
        }

        uint32_t message_length =
            (static_cast<uint32_t>(buffer[4]) << 24) | (static_cast<uint32_t>(buffer[5]) << 16) |
            (static_cast<uint32_t>(buffer[6]) << 8) | static_cast<uint32_t>(buffer[7]);

        if (message_length < 8 || message_length > MAX_MESSAGE_SIZE) {
            size_t search_start = 1;
            bool found_valid_header = false;

            while (search_start + SOMEIP_HEADER_SIZE <= buffer.size()) {
                uint32_t candidate_length =
                    (static_cast<uint32_t>(buffer[search_start + 4]) << 24) |
                    (static_cast<uint32_t>(buffer[search_start + 5]) << 16) |
                    (static_cast<uint32_t>(buffer[search_start + 6]) << 8) |
                    static_cast<uint32_t>(buffer[search_start + 7]);
                if (candidate_length >= 8 && candidate_length <= MAX_MESSAGE_SIZE) {
                    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(search_start));
                    found_valid_header = true;
                    break;
                }
                ++search_start;
            }

            if (!found_valid_header) {
                buffer.clear();
                return false;
            }
            continue;
        }

        size_t total_message_size = 8 + message_length;

        if (buffer.size() < total_message_size) {
            return false;
        }

        std::vector<uint8_t> message_data(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(total_message_size));
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(total_message_size));

        message = platform::allocate_message();
        if (message && message->deserialize(message_data)) {
            return true;
        }
    }
}

} // namespace transport
} // namespace someip
