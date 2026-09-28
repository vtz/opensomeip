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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "common/result.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::allocate_message from memory_impl.h
#include "platform/buffer_pool.h"
#include "platform/containers.h"
// NOLINTNEXTLINE(misc-include-cleaner) - platform::allocate_message from memory_impl.h
#include "platform/memory.h"
#include "platform/thread.h"
#include "someip/message.h"
#include "transport/endpoint.h"
#include "transport/message_rejection.h"
#include "transport/tcp_socket_adapter.h"
#include "transport/transport.h"

// NOLINTBEGIN(misc-include-cleaner)

namespace someip::transport {

const size_t EventDrivenTcpTransport::SOMEIP_HEADER_SIZE = 16;
const size_t EventDrivenTcpTransport::MAX_MESSAGE_SIZE = 65535;

EventDrivenTcpTransport::EventDrivenTcpTransport(ITcpSocketAdapter& adapter,
                                                 const EventDrivenTcpTransportConfig& config)
    : adapter_(adapter), config_(config)
{
}

EventDrivenTcpTransport::~EventDrivenTcpTransport()
{
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    stop();
}

Result EventDrivenTcpTransport::initialize(const Endpoint& local_endpoint)
{
    if (initialized_.load()) {
        return Result::INVALID_STATE;
    }

    local_endpoint_ =
        Endpoint(local_endpoint.get_address(), local_endpoint.get_port(), TransportProtocol::TCP);

    const Result result = adapter_.open(local_endpoint_);
    if (result != Result::SUCCESS) {
        return result;
    }

    local_endpoint_ = adapter_.get_local_endpoint();
    initialized_ = true;
    return Result::SUCCESS;
}

Result EventDrivenTcpTransport::enable_server_mode(int backlog)
{
    if (!initialized_.load()) {
        return Result::NOT_INITIALIZED;
    }
    const Result result = adapter_.listen(backlog);
    if (result != Result::SUCCESS) {
        return result;
    }
    server_mode_ = true;
    return Result::SUCCESS;
}

Result EventDrivenTcpTransport::try_accept_connection(Endpoint& remote_out)
{
    if (!server_mode_) {
        return Result::INVALID_STATE;
    }
    if (!running_.load()) {
        return Result::INVALID_STATE;
    }
    return adapter_.accept(remote_out);
}

Result EventDrivenTcpTransport::send_message(const Message& message, const Endpoint& /*endpoint*/)
{
    if (!is_connected()) {
        return Result::NOT_CONNECTED;
    }

    const auto data = message.serialize();
    return adapter_.send(data);
}

MessagePtr EventDrivenTcpTransport::receive_message()
{
    const platform::ScopedLock lock(queue_mutex_);
    if (message_queue_.empty()) {
        return nullptr;
    }
    MessagePtr message = message_queue_.front().first;
    message_queue_.pop();
    return message;
}

Result EventDrivenTcpTransport::connect(const Endpoint& endpoint)
{
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

Result EventDrivenTcpTransport::disconnect()
{
    if (!adapter_.is_connected() && !initialized_.load()) {
        return Result::SUCCESS;
    }
    adapter_.close();
    {
        const platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.clear();
    }
    initialized_ = false;
    return Result::SUCCESS;
}

bool EventDrivenTcpTransport::is_connected() const
{
    return adapter_.is_connected();
}

Endpoint EventDrivenTcpTransport::get_local_endpoint() const
{
    if (initialized_.load()) {
        return adapter_.get_local_endpoint();
    }
    return local_endpoint_;
}

void EventDrivenTcpTransport::set_listener(ITransportListener* listener)
{
    listener_.store(listener, std::memory_order_release);
}

Result EventDrivenTcpTransport::start()
{
    if (!initialized_.load()) {
        return Result::NOT_INITIALIZED;
    }
    if (running_.load()) {
        return Result::SUCCESS;
    }

    adapter_.set_receive_callback(
        [this](const platform::ByteBuffer& data) { on_adapter_receive(data); });
    adapter_.set_connected_callback(
        [this](const Endpoint& remote) { on_adapter_connected(remote); });
    adapter_.set_disconnected_callback([this]() { on_adapter_disconnected(); });

    running_ = true;
    return Result::SUCCESS;
}

Result EventDrivenTcpTransport::stop()
{
    running_ = false;

    adapter_.set_receive_callback(nullptr);
    adapter_.set_connected_callback(nullptr);
    adapter_.set_disconnected_callback(nullptr);

    adapter_.close();
    initialized_ = false;
    server_mode_ = false;

    {
        const platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.clear();
        while (!message_queue_.empty()) {
            message_queue_.pop();
        }
    }

    return Result::SUCCESS;
}

bool EventDrivenTcpTransport::is_running() const
{
    return running_.load();
}

void EventDrivenTcpTransport::testing_fail_next_allocations(size_t count)
{
    const platform::ScopedLock lock(queue_mutex_);
    fail_next_allocations_ = count;
}

/** @implements REQ_TRANSPORT_024, REQ_TRANSPORT_027 */
void EventDrivenTcpTransport::deliver_or_enqueue(const MessagePtr& message, const Endpoint& sender)
{
    ITransportListener* const cb = listener_.load(std::memory_order_acquire);
    if (cb != nullptr) {
        cb->on_message_received(message, sender);
    } else {
        const platform::ScopedLock lock(queue_mutex_);
        // NOLINTNEXTLINE(modernize-use-emplace,hicpp-use-emplace)
        message_queue_.push(std::pair<MessagePtr, Endpoint>{message, sender});
    }
}

/** @implements REQ_TRANSPORT_026, REQ_TRANSPORT_028 */
void EventDrivenTcpTransport::notify_rejection(const MessageRejectionInfo& info)
{
    ITransportListener* const cb = listener_.load(std::memory_order_acquire);
    if (cb != nullptr) {
        cb->on_message_rejected(info);
    }
}

/** @implements REQ_TRANSPORT_024, REQ_TRANSPORT_027, REQ_TRANSPORT_028, REQ_TRANSPORT_029, REQ_TRANSPORT_026, REQ_PAL_MEM_EXHAUST_E01 */
void EventDrivenTcpTransport::on_adapter_receive(const platform::ByteBuffer& data)
{
    if (!running_.load() || !initialized_.load()) {
        return;
    }

    {
        const platform::ScopedLock lock(queue_mutex_);
        if (!data.empty()) {
            receive_buffer_.insert(receive_buffer_.end(), data.data(), data.data() + data.size());
        }
    }

    for (;;) {
        MessagePtr message;
        Endpoint sender_ep;
        ParseOutcome outcome = ParseOutcome::NEED_MORE;
        Result rejection = Result::SUCCESS;
        MessageRejectionStage stage = MessageRejectionStage::DESERIALIZE;
        std::array<uint8_t, 12> header_prefix{};
        size_t prefix_len = 0;
        {
            const platform::ScopedLock lock(queue_mutex_);
            if (receive_buffer_.empty()) {
                break;
            }
            prefix_len = std::min(header_prefix.size(), receive_buffer_.size());
            std::copy_n(receive_buffer_.data(), prefix_len, header_prefix.data());
            outcome = parse_next_message(receive_buffer_, message, rejection, stage);
            sender_ep = connection_remote_;
        }
        if (outcome == ParseOutcome::NEED_MORE) {
            break;
        }
        if (outcome == ParseOutcome::CONTROL_FRAME) {
            continue;
        }
        if (outcome == ParseOutcome::REJECTED) {
            if (rejection == Result::OUT_OF_MEMORY) {
                ITransportListener* const cb = listener_.load(std::memory_order_acquire);
                if (cb != nullptr) {
                    cb->on_error(Result::OUT_OF_MEMORY);
                }
                continue;
            }
            MessageRejectionInfo info;
            info.sender = sender_ep;
            info.result = rejection;
            info.stage = stage;
            if (message) {
                fill_rejection_ids(info, *message, SOMEIP_HEADER_SIZE);
            } else {
                fill_rejection_ids(info, header_prefix.data(), prefix_len);
            }
            notify_rejection(info);
            continue;
        }
        deliver_or_enqueue(message, sender_ep);
    }
}

void EventDrivenTcpTransport::on_adapter_connected(const Endpoint& remote)
{
    if (!running_.load()) {
        return;
    }
    connection_remote_ = remote;
    ITransportListener* const cb = listener_.load(std::memory_order_acquire);
    if (cb != nullptr) {
        cb->on_connection_established(remote);
    }
}

void EventDrivenTcpTransport::on_adapter_disconnected()
{
    if (!running_.load()) {
        return;
    }
    const Endpoint lost = connection_remote_;
    {
        const platform::ScopedLock lock(queue_mutex_);
        receive_buffer_.clear();
    }
    ITransportListener* const cb = listener_.load(std::memory_order_acquire);
    if (cb != nullptr) {
        cb->on_connection_lost(lost);
    }
}

EventDrivenTcpTransport::ParseOutcome EventDrivenTcpTransport::parse_next_message(
    platform::ByteBuffer& buffer, MessagePtr& message, Result& rejection,
    MessageRejectionStage& stage)
{
    rejection = Result::SUCCESS;
    stage = MessageRejectionStage::TCP_FRAMING;
    message.reset();

    for (;;) {
        if (buffer.size() < SOMEIP_HEADER_SIZE) {
            // Retain only an incomplete trailer. If it already exceeds the
            // configured reassembly limit, discard and signal (REQ_TRANSPORT_028).
            if (buffer.size() > config_.max_receive_buffer) {
                buffer.clear();
                rejection = Result::BUFFER_OVERFLOW;
                return ParseOutcome::REJECTED;
            }
            return ParseOutcome::NEED_MORE;
        }

        if (is_magic_cookie(buffer, 0)) {
            buffer.erase(buffer.begin(),
                         buffer.begin() + static_cast<std::ptrdiff_t>(SOMEIP_HEADER_SIZE));
            return ParseOutcome::CONTROL_FRAME;
        }

        const uint32_t message_length =
            (static_cast<uint32_t>(buffer[4]) << 24U) | (static_cast<uint32_t>(buffer[5]) << 16U) |
            (static_cast<uint32_t>(buffer[6]) << 8U) | static_cast<uint32_t>(buffer[7]);

        if (message_length < 8 || message_length > MAX_MESSAGE_SIZE) {
            size_t search_start = 1;
            bool found_valid_header = false;

            while (search_start + SOMEIP_HEADER_SIZE <= buffer.size()) {
                if (is_magic_cookie(buffer, search_start)) {
                    buffer.erase(buffer.begin(),
                                 buffer.begin() + static_cast<std::ptrdiff_t>(search_start));
                    found_valid_header = true;
                    break;
                }
                const uint32_t candidate_length =
                    (static_cast<uint32_t>(buffer[search_start + 4]) << 24U) |
                    (static_cast<uint32_t>(buffer[search_start + 5]) << 16U) |
                    (static_cast<uint32_t>(buffer[search_start + 6]) << 8U) |
                    static_cast<uint32_t>(buffer[search_start + 7]);
                if (candidate_length >= 8 && candidate_length <= MAX_MESSAGE_SIZE) {
                    buffer.erase(buffer.begin(),
                                 buffer.begin() + static_cast<std::ptrdiff_t>(search_start));
                    found_valid_header = true;
                    break;
                }
                ++search_start;
            }

            if (!found_valid_header) {
                buffer.clear();
                rejection = Result::MALFORMED_MESSAGE;
                return ParseOutcome::REJECTED;
            }
            continue;
        }

        const size_t total_message_size = 8 + message_length;

        if (total_message_size > config_.max_receive_buffer) {
            buffer.clear();
            rejection = Result::BUFFER_OVERFLOW;
            return ParseOutcome::REJECTED;
        }

        if (buffer.size() < total_message_size) {
            if (buffer.size() > config_.max_receive_buffer) {
                buffer.clear();
                rejection = Result::BUFFER_OVERFLOW;
                return ParseOutcome::REJECTED;
            }
            return ParseOutcome::NEED_MORE;
        }

        const platform::ByteBuffer message_data(buffer.data(), buffer.data() + total_message_size);
        buffer.erase(buffer.begin(),
                     buffer.begin() + static_cast<std::ptrdiff_t>(total_message_size));

        if (fail_next_allocations_ > 0) {
            --fail_next_allocations_;
            rejection = Result::OUT_OF_MEMORY;
            return ParseOutcome::REJECTED;
        }

        message = platform::allocate_message();
        if (message == nullptr) {
            rejection = Result::OUT_OF_MEMORY;
            return ParseOutcome::REJECTED;
        }
        if (!message->deserialize(message_data)) {
            rejection = Result::MALFORMED_MESSAGE;
            stage = MessageRejectionStage::DESERIALIZE;
            return ParseOutcome::REJECTED;
        }
        return ParseOutcome::MESSAGE;
    }
}

/** @implements REQ_TRANSPORT_020, REQ_TRANSPORT_025 */
bool EventDrivenTcpTransport::is_magic_cookie(const platform::ByteBuffer& data, size_t offset)
{
    if (offset + SOMEIP_HEADER_SIZE > data.size()) {
        return false;
    }
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

}  // namespace someip::transport

// NOLINTEND(misc-include-cleaner)
