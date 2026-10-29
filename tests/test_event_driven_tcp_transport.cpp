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

#include <gtest/gtest.h>
#include <transport/event_driven_tcp_transport.h>
#include <transport/tcp_socket_adapter.h>
#include <transport/transport.h>
#include <someip/message.h>
#include <e2e/e2e_config.h>
#include <e2e/e2e_protection.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "static_pool_init.h"

using namespace someip;
using namespace someip::transport;

namespace {

class TestEventTcpListener : public ITransportListener {
public:
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::scoped_lock lock(mutex_);
        received_messages_.push_back({std::move(message), sender});
        cv_.notify_one();
    }

    void on_connection_lost(const Endpoint& endpoint) override {
        std::scoped_lock lock(mutex_);
        lost_endpoint_ = endpoint;
        connection_lost_count_++;
        cv_.notify_one();
    }

    void on_connection_established(const Endpoint& endpoint) override {
        std::scoped_lock lock(mutex_);
        established_endpoint_ = endpoint;
        connection_established_count_++;
        cv_.notify_one();
    }

    void on_error(Result error) override {
        std::scoped_lock lock(mutex_);
        last_error_ = error;
        error_count_++;
        cv_.notify_one();
    }

    void on_message_rejected(const MessageRejectionInfo& info) override {
        std::scoped_lock lock(mutex_);
        rejections_.push_back(info);
        cv_.notify_one();
    }

    bool wait_for_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return !received_messages_.empty(); });
    }

    bool wait_for_messages(size_t count,
                           std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout,
                            [this, count]() { return received_messages_.size() >= count; });
    }

    bool wait_for_error(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return error_count_ > 0; });
    }

    bool wait_for_rejection(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return !rejections_.empty(); });
    }

    bool wait_for_connection(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return connection_established_count_ > 0; });
    }

    bool wait_for_disconnect(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return connection_lost_count_ > 0; });
    }

    size_t message_count() const {
        std::scoped_lock lock(mutex_);
        return received_messages_.size();
    }

    uint16_t message_service_id(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first->get_service_id();
    }

    uint16_t message_session_id(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first->get_session_id();
    }

    const platform::ByteBuffer& message_payload(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first->get_payload();
    }

    bool message_has_e2e(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first->has_e2e_header();
    }

    MessagePtr message_at(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first;
    }

    size_t rejection_count() const {
        std::scoped_lock lock(mutex_);
        return rejections_.size();
    }

    Result last_rejection_result() const {
        std::scoped_lock lock(mutex_);
        return rejections_.empty() ? Result::SUCCESS : rejections_.back().result;
    }

    MessageRejectionStage last_rejection_stage() const {
        std::scoped_lock lock(mutex_);
        return rejections_.empty() ? MessageRejectionStage::DESERIALIZE
                                   : rejections_.back().stage;
    }

    std::atomic<int> connection_established_count_{0};
    std::atomic<int> connection_lost_count_{0};
    std::atomic<Result> last_error_{Result::SUCCESS};
    std::atomic<int> error_count_{0};
    Endpoint established_endpoint_{"0.0.0.0", 0};
    Endpoint lost_endpoint_{"0.0.0.0", 0};

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::pair<MessagePtr, Endpoint>> received_messages_;
    std::vector<MessageRejectionInfo> rejections_;
};

class MockTcpAdapter : public ITcpSocketAdapter {
public:
    Result open(const Endpoint& local_endpoint) override {
        if (open_result_ != Result::SUCCESS) {
            return open_result_;
        }
        open_ = true;
        local_ = local_endpoint;
        if (local_.get_port() == 0) {
            local_.set_port(54321);
        }
        return Result::SUCCESS;
    }

    void close() override {
        open_ = false;
        connected_ = false;
        receive_cb_ = nullptr;
        connected_cb_ = nullptr;
        disconnected_cb_ = nullptr;
    }

    Result listen(int /*backlog*/) override {
        if (!open_) {
            return Result::INVALID_STATE;
        }
        listening_ = true;
        return Result::SUCCESS;
    }

    Result connect(const Endpoint& remote_endpoint) override {
        if (connect_result_ != Result::SUCCESS) {
            return connect_result_;
        }
        connected_ = true;
        remote_ = remote_endpoint;
        if (connected_cb_) {
            connected_cb_(remote_endpoint);
        }
        return Result::SUCCESS;
    }

    Result accept(Endpoint& remote_out) override {
        if (!listening_) {
            return Result::INVALID_STATE;
        }
        if (!pending_connection_) {
            return Result::TIMEOUT;
        }
        remote_out = pending_remote_;
        connected_ = true;
        pending_connection_ = false;
        return Result::SUCCESS;
    }

    Result send(const platform::ByteBuffer& data) override {
        if (!connected_) {
            return Result::NOT_CONNECTED;
        }
        last_send_data_ = data;
        return Result::SUCCESS;
    }

    void set_receive_callback(TcpReceiveCallback callback) override {
        receive_cb_ = std::move(callback);
    }

    void set_connected_callback(TcpConnectedCallback callback) override {
        connected_cb_ = std::move(callback);
    }

    void set_disconnected_callback(TcpDisconnectedCallback callback) override {
        disconnected_cb_ = std::move(callback);
    }

    Endpoint get_local_endpoint() const override { return local_; }
    bool is_connected() const override { return connected_; }

    void inject_receive(const platform::ByteBuffer& data) {
        if (receive_cb_) {
            receive_cb_(data);
        }
    }

    void inject_connected(const Endpoint& remote) {
        connected_ = true;
        remote_ = remote;
        if (connected_cb_) {
            connected_cb_(remote);
        }
    }

    void inject_disconnected() {
        connected_ = false;
        if (disconnected_cb_) {
            disconnected_cb_();
        }
    }

    void set_open_result(Result r) { open_result_ = r; }
    void set_connect_result(Result r) { connect_result_ = r; }

    void stage_pending_connection(const Endpoint& remote) {
        pending_connection_ = true;
        pending_remote_ = remote;
    }

    bool is_open() const { return open_; }
    platform::ByteBuffer last_send_data_;

private:
    bool open_{false};
    bool connected_{false};
    bool listening_{false};
    bool pending_connection_{false};
    Endpoint local_{"127.0.0.1", 0};
    Endpoint remote_{"0.0.0.0", 0};
    Endpoint pending_remote_{"0.0.0.0", 0};
    TcpReceiveCallback receive_cb_;
    TcpConnectedCallback connected_cb_;
    TcpDisconnectedCallback disconnected_cb_;
    Result open_result_{Result::SUCCESS};
    Result connect_result_{Result::SUCCESS};
};

Message make_tcp_sample_message(uint16_t service_id = 0x1234, uint16_t session_id = 0xDEF0) {
    Message message;
    message.set_service_id(service_id);
    message.set_method_id(0x5678);
    message.set_client_id(0x9ABC);
    message.set_session_id(session_id);
    message.set_protocol_version(1);
    message.set_interface_version(1);
    message.set_message_type(MessageType::REQUEST);
    message.set_return_code(ReturnCode::E_OK);
    message.set_payload({0x01, 0x02, 0x03});
    return message;
}

platform::ByteBuffer concatenate_frames(const std::vector<Message>& messages) {
    platform::ByteBuffer combined;
    for (const Message& message : messages) {
        platform::ByteBuffer raw = message.serialize();
        combined.insert(combined.end(), raw.begin(), raw.end());
    }
    return combined;
}

} // namespace

TEST(EventDrivenTcpTransport, ConstructionNotRunning) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    EXPECT_FALSE(transport.is_running());
    EXPECT_FALSE(transport.is_connected());
}

TEST(EventDrivenTcpTransport, InitializeAndStart) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    Endpoint local{"127.0.0.1", 30490, TransportProtocol::TCP};
    EXPECT_EQ(transport.initialize(local), Result::SUCCESS);
    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());
    EXPECT_EQ(transport.get_local_endpoint().get_port(), 30490);

    transport.stop();
    EXPECT_FALSE(transport.is_running());
}

TEST(EventDrivenTcpTransport, StartWithoutInitializeFails) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    EXPECT_EQ(transport.start(), Result::NOT_INITIALIZED);
    EXPECT_FALSE(transport.is_running());
}

TEST(EventDrivenTcpTransport, InitializePropagatesOpenFailure) {
    MockTcpAdapter adapter;
    adapter.set_open_result(Result::NETWORK_ERROR);
    EventDrivenTcpTransport transport(adapter);

    EXPECT_EQ(transport.initialize(Endpoint{"127.0.0.1", 30490}), Result::NETWORK_ERROR);
}

TEST(EventDrivenTcpTransport, DoubleInitializeFails) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    EXPECT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    EXPECT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::INVALID_STATE);
}

TEST(EventDrivenTcpTransport, ConnectCallbackNotifiesListener) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Endpoint remote{"10.0.0.1", 5000, TransportProtocol::TCP};
    adapter.inject_connected(remote);

    ASSERT_TRUE(listener.wait_for_connection());
    EXPECT_EQ(listener.connection_established_count_.load(), 1);

    transport.stop();
}

TEST(EventDrivenTcpTransport, DisconnectCallbackNotifiesListener) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Endpoint remote{"10.0.0.1", 5000, TransportProtocol::TCP};
    adapter.inject_connected(remote);
    ASSERT_TRUE(listener.wait_for_connection());

    adapter.inject_disconnected();
    ASSERT_TRUE(listener.wait_for_disconnect());
    EXPECT_EQ(listener.connection_lost_count_.load(), 1);

    transport.stop();
}

TEST(EventDrivenTcpTransport, SendForwardsToAdapter) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message msg = make_tcp_sample_message();
    Endpoint dest{"10.0.0.1", 5000};
    ASSERT_EQ(transport.send_message(msg, dest), Result::SUCCESS);

    platform::ByteBuffer expected = msg.serialize();
    EXPECT_EQ(adapter.last_send_data_, expected);

    transport.stop();
}

TEST(EventDrivenTcpTransport, SendWhenNotConnectedFails) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Message msg = make_tcp_sample_message();
    EXPECT_EQ(transport.send_message(msg, Endpoint{"10.0.0.1", 5000}), Result::NOT_CONNECTED);

    transport.stop();
}

TEST(EventDrivenTcpTransport, ReceiveCallbackReassemblesMessage) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message sent = make_tcp_sample_message();
    platform::ByteBuffer raw = sent.serialize();
    adapter.inject_receive(raw);

    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_service_id(0), sent.get_service_id());

    MessagePtr queued = transport.receive_message();
    EXPECT_EQ(queued, nullptr);

    transport.stop();
}

TEST(EventDrivenTcpTransport, ReceiveFragmentedMessage) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message sent = make_tcp_sample_message();
    platform::ByteBuffer raw = sent.serialize();

    size_t half = raw.size() / 2;
    platform::ByteBuffer first_half(raw.data(), raw.data() + half);
    platform::ByteBuffer second_half(raw.data() + half, raw.data() + raw.size());

    adapter.inject_receive(first_half);
    EXPECT_FALSE(listener.wait_for_message(std::chrono::milliseconds(50)));

    adapter.inject_receive(second_half);
    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_service_id(0), sent.get_service_id());

    transport.stop();
}

TEST(EventDrivenTcpTransport, ServerModeEnableAndAccept) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 30490}), Result::SUCCESS);
    ASSERT_EQ(transport.enable_server_mode(), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Endpoint remote{"10.0.0.2", 40000, TransportProtocol::TCP};
    adapter.stage_pending_connection(remote);

    Endpoint accepted;
    EXPECT_EQ(transport.try_accept_connection(accepted), Result::SUCCESS);
    EXPECT_EQ(accepted, remote);

    transport.stop();
}

TEST(EventDrivenTcpTransport, ServerModeWithoutInitializeFails) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    EXPECT_EQ(transport.enable_server_mode(), Result::NOT_INITIALIZED);
}

TEST(EventDrivenTcpTransport, ConnectInServerModeFails) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.enable_server_mode(), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    EXPECT_EQ(transport.connect(Endpoint{"10.0.0.1", 5000}), Result::INVALID_STATE);

    transport.stop();
}

TEST(EventDrivenTcpTransport, ListenerPreservedAcrossStopStart) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    transport.stop();

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message sent = make_tcp_sample_message();
    adapter.inject_receive(sent.serialize());

    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);

    transport.stop();
}

TEST(EventDrivenTcpTransport, StopClearsCallbacks) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    transport.stop();

    adapter.inject_receive({0x00, 0x01, 0x02});

    EXPECT_FALSE(listener.wait_for_message(std::chrono::milliseconds(50)));
}

TEST(EventDrivenTcpTransport, MagicCookieIsSkipped) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    platform::ByteBuffer client_cookie = {
        0xFF, 0xFF, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08,
        0xDE, 0xAD, 0xBE, 0xEF,
        0x01, 0x01, 0x01, 0x00
    };

    Message sent = make_tcp_sample_message();
    platform::ByteBuffer raw = sent.serialize();

    platform::ByteBuffer combined;
    combined.insert(combined.end(), client_cookie.begin(), client_cookie.end());
    combined.insert(combined.end(), raw.begin(), raw.end());

    adapter.inject_receive(combined);

    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_service_id(0), sent.get_service_id());

    transport.stop();
}

TEST(EventDrivenTcpTransport, ServerMagicCookieIsSkipped) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    platform::ByteBuffer server_cookie = {
        0xFF, 0xFF, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x08,
        0xDE, 0xAD, 0xBE, 0xEF,
        0x01, 0x01, 0x02, 0x00
    };

    Message sent = make_tcp_sample_message();
    platform::ByteBuffer raw = sent.serialize();

    platform::ByteBuffer combined;
    combined.insert(combined.end(), server_cookie.begin(), server_cookie.end());
    combined.insert(combined.end(), raw.begin(), raw.end());

    adapter.inject_receive(combined);

    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_service_id(0), sent.get_service_id());

    transport.stop();
}

/**
 * @test_case TC_ED_TCP_NPDU_001
 * @tests REQ_TRANSPORT_027, REQ_TRANSPORT_024
 * @brief One adapter callback with more than 32 concatenated frames delivers all in order.
 *
 * The listener records session IDs and drops the MessagePtr so a 16-slot static
 * pool can recycle across the burst. Parse-one/deliver-one avoids a bounded
 * staging vector that could not hold 40 frames even when the pool could recycle.
 */
TEST(EventDrivenTcpTransport, ConcatenatedBurstDeliversAllInOrder) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    class BurstListener : public ITransportListener {
    public:
        void on_message_received(MessagePtr message, const Endpoint& /*sender*/) override {
            std::scoped_lock lock(mutex_);
            sessions_.push_back(message->get_session_id());
            cv_.notify_one();
        }
        void on_connection_lost(const Endpoint& /*endpoint*/) override {}
        void on_connection_established(const Endpoint& /*endpoint*/) override {}
        void on_error(Result error) override {
            std::scoped_lock lock(mutex_);
            last_error_ = error;
            error_count_++;
            cv_.notify_one();
        }

        bool wait_for(size_t count,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
            std::unique_lock lock(mutex_);
            return cv_.wait_for(lock, timeout, [this, count]() { return sessions_.size() >= count; });
        }

        std::vector<uint16_t> sessions() const {
            std::scoped_lock lock(mutex_);
            return sessions_;
        }

        std::atomic<int> error_count_{0};
        std::atomic<Result> last_error_{Result::SUCCESS};

    private:
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::vector<uint16_t> sessions_;
    } listener;

    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    constexpr size_t kFrameCount = 40;
    std::vector<Message> messages;
    messages.reserve(kFrameCount);
    for (size_t i = 0; i < kFrameCount; ++i) {
        messages.push_back(make_tcp_sample_message(0x1234, static_cast<uint16_t>(i + 1)));
    }
    adapter.inject_receive(concatenate_frames(messages));

    ASSERT_TRUE(listener.wait_for(kFrameCount));
    std::vector<uint16_t> sessions = listener.sessions();
    ASSERT_EQ(sessions.size(), kFrameCount);
    for (size_t i = 0; i < kFrameCount; ++i) {
        EXPECT_EQ(sessions[i], static_cast<uint16_t>(i + 1));
    }
    EXPECT_EQ(transport.receive_message(), nullptr);
    EXPECT_EQ(listener.error_count_.load(), 0);

    transport.stop();
}

/**
 * @test_case TC_ED_TCP_QUEUE_001
 * @tests REQ_TRANSPORT_027
 * @brief Without a listener, concatenated frames are queued in order and not dropped.
 */
TEST(EventDrivenTcpTransport, ConcatenatedBurstEnqueuedWithoutListener) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    std::vector<Message> messages = {
        make_tcp_sample_message(0x1111, 1),
        make_tcp_sample_message(0x2222, 2),
        make_tcp_sample_message(0x3333, 3),
    };
    adapter.inject_receive(concatenate_frames(messages));

    for (size_t i = 0; i < messages.size(); ++i) {
        MessagePtr queued = transport.receive_message();
        ASSERT_NE(queued, nullptr);
        EXPECT_EQ(queued->get_service_id(), messages[i].get_service_id());
        EXPECT_EQ(queued->get_session_id(), messages[i].get_session_id());
    }
    EXPECT_EQ(transport.receive_message(), nullptr);

    transport.stop();
}

/**
 * @test_case TC_ED_TCP_FRAG_001
 * @tests REQ_TRANSPORT_027, REQ_TRANSPORT_028
 * @brief Incomplete trailing frame is left for the next adapter callback.
 */
TEST(EventDrivenTcpTransport, IncompleteTrailingFrameLeftForNextCallback) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message first = make_tcp_sample_message(0x1001, 1);
    Message second = make_tcp_sample_message(0x1002, 2);
    Message third = make_tcp_sample_message(0x1003, 3);
    platform::ByteBuffer first_raw = first.serialize();
    platform::ByteBuffer second_raw = second.serialize();
    platform::ByteBuffer third_raw = third.serialize();

    platform::ByteBuffer combined;
    combined.insert(combined.end(), first_raw.begin(), first_raw.end());
    combined.insert(combined.end(), second_raw.begin(), second_raw.end());
    const size_t partial = third_raw.size() / 2;
    combined.insert(combined.end(), third_raw.begin(),
                    third_raw.begin() + static_cast<std::ptrdiff_t>(partial));

    adapter.inject_receive(combined);
    ASSERT_TRUE(listener.wait_for_messages(2));
    ASSERT_EQ(listener.message_count(), 2u);
    EXPECT_EQ(listener.message_session_id(0), 1);
    EXPECT_EQ(listener.message_session_id(1), 2);

    platform::ByteBuffer rest(third_raw.begin() + static_cast<std::ptrdiff_t>(partial),
                              third_raw.end());
    adapter.inject_receive(rest);
    ASSERT_TRUE(listener.wait_for_messages(3));
    ASSERT_EQ(listener.message_count(), 3u);
    EXPECT_EQ(listener.message_session_id(2), 3);

    transport.stop();
}

/**
 * @test_case TC_ED_TCP_OOM_001
 * @tests REQ_TRANSPORT_029, REQ_PAL_MEM_EXHAUST_E01
 * @brief Pool exhaustion consumes the current complete frame, notifies OOM, and keeps parsing.
 */
TEST(EventDrivenTcpTransport, AllocFailureConsumesFrameAndNotifiesOom) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    transport.testing_fail_next_allocations(1);

    std::vector<Message> messages = {
        make_tcp_sample_message(0x2001, 1),
        make_tcp_sample_message(0x2002, 2),
        make_tcp_sample_message(0x2003, 3),
    };
    adapter.inject_receive(concatenate_frames(messages));

    ASSERT_TRUE(listener.wait_for_error());
    EXPECT_EQ(listener.last_error_.load(), Result::OUT_OF_MEMORY);
    EXPECT_EQ(listener.error_count_.load(), 1);
    ASSERT_TRUE(listener.wait_for_messages(2));
    ASSERT_EQ(listener.message_count(), 2u);
    EXPECT_EQ(listener.message_session_id(0), 2);
    EXPECT_EQ(listener.message_session_id(1), 3);
    EXPECT_EQ(listener.rejection_count(), 0u);

    adapter.inject_receive(make_tcp_sample_message(0x2004, 4).serialize());
    ASSERT_TRUE(listener.wait_for_messages(3));
    EXPECT_EQ(listener.message_session_id(2), 4);
    EXPECT_EQ(listener.error_count_.load(), 1);

    transport.stop();
}

/**
 * @test_case TC_ED_TCP_REJECT_001
 * @tests REQ_TRANSPORT_026
 * @brief A complete malformed frame is rejected and does not stall later frames.
 */
TEST(EventDrivenTcpTransport, CompleteMalformedFrameNotifiesRejection) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message bad = make_tcp_sample_message(0x3001, 1);
    bad.set_protocol_version(0x99);
    Message good = make_tcp_sample_message(0x3002, 2);
    adapter.inject_receive(concatenate_frames({bad, good}));

    ASSERT_TRUE(listener.wait_for_messages(1));
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_session_id(0), 2);
    ASSERT_EQ(listener.rejection_count(), 1u);

    transport.stop();
}

/**
 * @test_case TC_ED_TCP_BUF_001
 * @tests REQ_TRANSPORT_028, REQ_TRANSPORT_027
 * @brief Complete frames are delivered before buffer exhaustion resets and signals.
 *
 * A single callback carries two complete frames plus a Length-declared frame
 * larger than max_receive_buffer. The complete frames must be delivered in
 * order; the oversized declaration discards the reassembly buffer and reports
 * BUFFER_OVERFLOW via on_message_rejected (not silent, not on_error).
 */
TEST(EventDrivenTcpTransport, ReceiveBufferExhaustionResetsAndSignals) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransportConfig config;
    config.max_receive_buffer = 64;
    EventDrivenTcpTransport transport(adapter, config);
    TestEventTcpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message first = make_tcp_sample_message(0x4001, 1);
    Message second = make_tcp_sample_message(0x4002, 2);
    platform::ByteBuffer first_raw = first.serialize();
    platform::ByteBuffer second_raw = second.serialize();
    ASSERT_LT(first_raw.size() + second_raw.size(), config.max_receive_buffer);

    // Declared Length implies a frame larger than max_receive_buffer.
    platform::ByteBuffer oversized_header = {
        0x40, 0x03, 0x00, 0x01,  // service/method
        0x00, 0x00, 0x01, 0x00,  // length = 256 → total 264 bytes
        0x00, 0x01, 0x00, 0x03,  // client/session
        0x01, 0x01, 0x00, 0x00   // proto/iface/type/return
    };

    platform::ByteBuffer combined;
    combined.insert(combined.end(), first_raw.begin(), first_raw.end());
    combined.insert(combined.end(), second_raw.begin(), second_raw.end());
    combined.insert(combined.end(), oversized_header.begin(), oversized_header.end());
    // Pad past max_receive_buffer so a regress-to-early-clear would also drop
    // the complete frames if it ran before parse-one/deliver-one.
    while (combined.size() <= config.max_receive_buffer + oversized_header.size()) {
        combined.push_back(0xAB);
    }

    adapter.inject_receive(combined);

    ASSERT_TRUE(listener.wait_for_messages(2));
    ASSERT_EQ(listener.message_count(), 2u);
    EXPECT_EQ(listener.message_session_id(0), 1);
    EXPECT_EQ(listener.message_session_id(1), 2);

    ASSERT_TRUE(listener.wait_for_rejection());
    ASSERT_EQ(listener.rejection_count(), 1u);
    EXPECT_EQ(listener.last_rejection_result(), Result::BUFFER_OVERFLOW);
    EXPECT_EQ(listener.last_rejection_stage(), MessageRejectionStage::TCP_FRAMING);
    EXPECT_EQ(listener.error_count_.load(), 0);

    // Stream continues after reset: a later complete frame still delivers.
    adapter.inject_receive(make_tcp_sample_message(0x4004, 4).serialize());
    ASSERT_TRUE(listener.wait_for_messages(3));
    EXPECT_EQ(listener.message_session_id(2), 4);
    EXPECT_EQ(listener.rejection_count(), 1u);

    transport.stop();
}

/**
 * @brief Listener may call stop() from on_message_received without deadlocking.
 *
 * Assumption: the adapter is single-threaded (callbacks are not concurrent) and
 * clearing callbacks from inside the active receive callback returns without
 * waiting for that same callback to finish (see ITcpSocketAdapter quiescence).
 */
TEST(EventDrivenTcpTransport, StopFromListenerDoesNotDeadlock) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransport transport(adapter);

    class StoppingListener : public ITransportListener {
    public:
        explicit StoppingListener(EventDrivenTcpTransport& transport) : transport_(transport) {}

        void on_message_received(MessagePtr /*message*/, const Endpoint& /*sender*/) override {
            ++messages_;
            stop_result_ = transport_.stop();
            stopped_ = true;
        }

        void on_connection_lost(const Endpoint& /*endpoint*/) override {}
        void on_connection_established(const Endpoint& /*endpoint*/) override {}
        void on_error(Result /*error*/) override {}

        EventDrivenTcpTransport& transport_;
        int messages_{0};
        bool stopped_{false};
        Result stop_result_{Result::SUCCESS};
    } listener(transport);

    transport.set_listener(&listener);
    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    adapter.inject_receive(make_tcp_sample_message().serialize());

    EXPECT_EQ(listener.messages_, 1);
    EXPECT_TRUE(listener.stopped_);
    EXPECT_EQ(listener.stop_result_, Result::SUCCESS);
    EXPECT_FALSE(transport.is_running());
}

TEST(EventDrivenTcpTransport, ApplicationManagedE2EExposesMetadata) {
    MockTcpAdapter adapter;
    EventDrivenTcpTransportConfig cfg;
    e2e::E2EReceiveBinding binding;
    binding.service_id = 0x1234;
    binding.method_id = 0x5678;
    binding.data_id = 0x0100;
    binding.policy = e2e::E2EReceivePolicy::APPLICATION_MANAGED;
    binding.enable_freshness = false;
    ASSERT_EQ(cfg.e2e_receive.add(binding), Result::SUCCESS);

    EventDrivenTcpTransport transport(adapter, cfg);
    TestEventTcpListener listener;
    transport.set_listener(&listener);
    ASSERT_EQ(transport.initialize(Endpoint{"127.0.0.1", 0}), Result::SUCCESS);
    ASSERT_EQ(transport.start(), Result::SUCCESS);
    adapter.inject_connected(Endpoint{"10.0.0.1", 5000, TransportProtocol::TCP});

    Message sent = make_tcp_sample_message();
    e2e::E2EConfig e2e_config(0x0100);
    e2e_config.enable_freshness = false;
    e2e::E2EProtection protection;
    ASSERT_EQ(protection.protect(sent, e2e_config), Result::SUCCESS);
    platform::ByteBuffer flipped = sent.get_payload();
    flipped[0] ^= 0xFFU;
    sent.set_payload(flipped);
    adapter.inject_receive(sent.serialize());

    ASSERT_TRUE(listener.wait_for_message());
    EXPECT_EQ(listener.rejection_count(), 0u);
    EXPECT_EQ(listener.message_payload(0), flipped);
    EXPECT_TRUE(listener.message_has_e2e(0));
    EXPECT_EQ(protection.validate(*listener.message_at(0), e2e_config), Result::INVALID_ARGUMENT);

    transport.stop();
}
