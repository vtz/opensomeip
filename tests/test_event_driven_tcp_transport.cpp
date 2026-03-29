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
#include <atomic>
#include <mutex>
#include <condition_variable>

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

    bool wait_for_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return !received_messages_.empty(); });
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

    Result send(const std::vector<uint8_t>& data) override {
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

    void inject_receive(const std::vector<uint8_t>& data) {
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
    std::vector<uint8_t> last_send_data_;

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

Message make_tcp_sample_message() {
    Message message;
    message.set_service_id(0x1234);
    message.set_method_id(0x5678);
    message.set_client_id(0x9ABC);
    message.set_session_id(0xDEF0);
    message.set_protocol_version(1);
    message.set_interface_version(1);
    message.set_message_type(MessageType::REQUEST);
    message.set_return_code(ReturnCode::E_OK);
    message.set_payload({0x01, 0x02, 0x03});
    return message;
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

    std::vector<uint8_t> expected = msg.serialize();
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
    std::vector<uint8_t> raw = sent.serialize();
    adapter.inject_receive(raw);

    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_service_id(0), sent.get_service_id());

    MessagePtr queued = transport.receive_message();
    ASSERT_NE(queued, nullptr);
    EXPECT_EQ(queued->get_method_id(), sent.get_method_id());

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
    std::vector<uint8_t> raw = sent.serialize();

    size_t half = raw.size() / 2;
    std::vector<uint8_t> first_half(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(half));
    std::vector<uint8_t> second_half(raw.begin() + static_cast<std::ptrdiff_t>(half), raw.end());

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
