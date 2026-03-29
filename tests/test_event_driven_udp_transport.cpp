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
#include <transport/event_driven_udp_transport.h>
#include <transport/udp_socket_adapter.h>
#include <transport/transport.h>
#include <someip/message.h>
#include <atomic>
#include <mutex>
#include <condition_variable>

using namespace someip;
using namespace someip::transport;

namespace {

class TestEventUdpListener : public ITransportListener {
public:
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::scoped_lock lock(mutex_);
        received_messages_.push_back({std::move(message), sender});
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

    bool wait_for_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return !received_messages_.empty(); });
    }

    bool wait_for_error(std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return error_count_ > 0; });
    }

    size_t message_count() const {
        std::scoped_lock lock(mutex_);
        return received_messages_.size();
    }

    Endpoint message_sender(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).second;
    }

    uint16_t message_service_id(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first->get_service_id();
    }

    uint16_t message_method_id(size_t index) const {
        std::scoped_lock lock(mutex_);
        return received_messages_.at(index).first->get_method_id();
    }

    std::atomic<Result> last_error_{Result::SUCCESS};
    std::atomic<int> error_count_{0};

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::pair<MessagePtr, Endpoint>> received_messages_;
};

class MockUdpAdapter : public IUdpSocketAdapter {
public:
    void set_open_result(Result r) { open_result_ = r; }

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
        cb_ = nullptr;
    }

    Result send(const std::vector<uint8_t>& data, const Endpoint& destination) override {
        last_send_data_ = data;
        last_send_dest_ = destination;
        return Result::SUCCESS;
    }

    Result join_multicast(const std::string& multicast_address,
                          const std::string& /*interface_address*/) override {
        joins_.push_back(multicast_address);
        return Result::SUCCESS;
    }

    Result leave_multicast(const std::string& multicast_address,
                           const std::string& /*interface_address*/) override {
        leaves_.push_back(multicast_address);
        return Result::SUCCESS;
    }

    void set_receive_callback(UdpReceiveCallback callback) override { cb_ = std::move(callback); }

    Endpoint get_local_endpoint() const override { return local_; }

    void inject_receive(const std::vector<uint8_t>& data, const Endpoint& sender) {
        if (cb_) {
            cb_(data, sender);
        }
    }

    bool is_open() const { return open_; }

    std::vector<uint8_t> last_send_data_;
    Endpoint last_send_dest_{"0.0.0.0", 0};
    std::vector<std::string> joins_;
    std::vector<std::string> leaves_;

private:
    bool open_{false};
    Endpoint local_{"127.0.0.1", 0};
    UdpReceiveCallback cb_;
    Result open_result_{Result::SUCCESS};
};

Message make_sample_message() {
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

TEST(EventDrivenUdpTransport, ConstructionNotRunning) {
    MockUdpAdapter adapter;
    Endpoint local{"127.0.0.1", 0};
    EventDrivenUdpTransport transport(adapter, local);

    EXPECT_FALSE(transport.is_running());
    EXPECT_FALSE(transport.is_connected());
    EXPECT_EQ(transport.get_local_endpoint().get_address(), "127.0.0.1");
}

TEST(EventDrivenUdpTransport, StartStopLifecycle) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});

    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());
    EXPECT_TRUE(transport.is_connected());
    EXPECT_TRUE(adapter.is_open());
    EXPECT_EQ(transport.get_local_endpoint().get_port(), 54321);

    EXPECT_EQ(transport.stop(), Result::SUCCESS);
    EXPECT_FALSE(transport.is_running());
    EXPECT_FALSE(adapter.is_open());
}

TEST(EventDrivenUdpTransport, StartPropagatesOpenFailure) {
    MockUdpAdapter adapter;
    adapter.set_open_result(Result::NETWORK_ERROR);
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 30490});

    EXPECT_EQ(transport.start(), Result::NETWORK_ERROR);
    EXPECT_FALSE(transport.is_running());
}

TEST(EventDrivenUdpTransport, SendForwardsToAdapter) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Message msg = make_sample_message();
    Endpoint dest{"127.0.0.1", 40000};
    ASSERT_EQ(transport.send_message(msg, dest), Result::SUCCESS);

    EXPECT_EQ(adapter.last_send_dest_, dest);
    std::vector<uint8_t> expected = msg.serialize();
    EXPECT_EQ(adapter.last_send_data_, expected);

    transport.stop();
}

TEST(EventDrivenUdpTransport, ReceiveCallbackNotifiesListener) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});
    TestEventUdpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Message sent = make_sample_message();
    std::vector<uint8_t> raw = sent.serialize();
    Endpoint sender{"10.0.0.5", 5000, TransportProtocol::UDP};
    adapter.inject_receive(raw, sender);

    ASSERT_TRUE(listener.wait_for_message());
    ASSERT_EQ(listener.message_count(), 1u);
    EXPECT_EQ(listener.message_sender(0), sender);
    EXPECT_EQ(listener.message_service_id(0), sent.get_service_id());

    MessagePtr queued = transport.receive_message();
    ASSERT_NE(queued, nullptr);
    EXPECT_EQ(queued->get_method_id(), sent.get_method_id());

    transport.stop();
}

TEST(EventDrivenUdpTransport, MalformedDatagramInvokesOnError) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});
    TestEventUdpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.start(), Result::SUCCESS);

    adapter.inject_receive({0x00, 0x01}, Endpoint{"127.0.0.1", 1234});

    ASSERT_TRUE(listener.wait_for_error());
    EXPECT_EQ(listener.last_error_.load(), Result::INVALID_MESSAGE);

    transport.stop();
}

TEST(EventDrivenUdpTransport, MulticastJoinLeave) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    EXPECT_EQ(transport.join_multicast_group("224.0.0.251"), Result::SUCCESS);
    EXPECT_EQ(transport.leave_multicast_group("224.0.0.251"), Result::SUCCESS);
    ASSERT_EQ(adapter.joins_.size(), 1u);
    ASSERT_EQ(adapter.leaves_.size(), 1u);
    EXPECT_EQ(adapter.joins_[0], "224.0.0.251");
    EXPECT_EQ(adapter.leaves_[0], "224.0.0.251");

    transport.stop();
}

TEST(EventDrivenUdpTransport, ConnectMulticastUdpJoinsAdapter) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});
    ASSERT_EQ(transport.start(), Result::SUCCESS);

    Endpoint mc("239.0.0.1", 30490, TransportProtocol::MULTICAST_UDP);
    EXPECT_EQ(transport.connect(mc), Result::SUCCESS);
    ASSERT_EQ(adapter.joins_.size(), 1u);
    EXPECT_EQ(adapter.joins_[0], "239.0.0.1");

    transport.stop();
}

TEST(EventDrivenUdpTransport, StopClearsReceiveCallback) {
    MockUdpAdapter adapter;
    EventDrivenUdpTransport transport(adapter, Endpoint{"127.0.0.1", 0});
    TestEventUdpListener listener;
    transport.set_listener(&listener);

    ASSERT_EQ(transport.start(), Result::SUCCESS);
    transport.stop();

    Message sent = make_sample_message();
    adapter.inject_receive(sent.serialize(), Endpoint{"127.0.0.1", 1});

    EXPECT_FALSE(listener.wait_for_message(std::chrono::milliseconds(50)));
}
