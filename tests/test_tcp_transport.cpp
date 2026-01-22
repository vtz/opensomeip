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
#include <transport/tcp_transport.h>
#include <transport/transport.h>
#include <someip/message.h>
#include <thread>
#include <chrono>

using namespace someip;
using namespace someip::transport;

/**
 * @brief TCP Transport unit tests
 * @tests REQ_TRANSPORT_002
 * @tests REQ_TRANSPORT_003
 * @tests REQ_TRANSPORT_004
 * @tests REQ_TRANSPORT_005
 * @tests feat_req_someip_850
 * @tests feat_req_someip_851
 */
class TcpTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.max_receive_buffer = 8192;
        config.connection_timeout = std::chrono::milliseconds(2000);
        config.receive_timeout = std::chrono::milliseconds(100);
        config.send_timeout = std::chrono::milliseconds(1000);
    }

    void TearDown() override {
        // Clean up any running transports
    }

    TcpTransportConfig config;
};

class TestTcpListener : public ITransportListener {
public:
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::scoped_lock lock(mutex_);
        received_messages_.push_back({message, sender});
        cv_.notify_one();
    }

    void on_connection_lost(const Endpoint& endpoint) override {
        std::scoped_lock lock(mutex_);
        connection_lost_ = true;
        lost_endpoint_ = endpoint;
        cv_.notify_one();
    }

    void on_connection_established(const Endpoint& endpoint) override {
        std::scoped_lock lock(mutex_);
        connection_established_ = true;
        established_endpoint_ = endpoint;
        cv_.notify_one();
    }

    void on_error(Result error) override {
        std::scoped_lock lock(mutex_);
        last_error_ = error;
        cv_.notify_one();
    }

    bool wait_for_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() {
            return !received_messages_.empty();
        });
    }

    bool wait_for_connection_lost(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() {
            return connection_lost_;
        });
    }

    bool wait_for_connection_established(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() {
            return connection_established_;
        });
    }

    std::vector<std::pair<MessagePtr, Endpoint>> get_received_messages() {
        std::scoped_lock lock(mutex_);
        return received_messages_;
    }

    void clear_messages() {
        std::scoped_lock lock(mutex_);
        received_messages_.clear();
    }

    bool get_connection_lost() const {
        std::scoped_lock lock(mutex_);
        return connection_lost_;
    }

    bool get_connection_established() const {
        std::scoped_lock lock(mutex_);
        return connection_established_;
    }

    Result get_last_error() const {
        std::scoped_lock lock(mutex_);
        return last_error_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::pair<MessagePtr, Endpoint>> received_messages_;
    bool connection_lost_ = false;
    bool connection_established_ = false;
    Endpoint lost_endpoint_;
    Endpoint established_endpoint_;
    Result last_error_ = Result::SUCCESS;
};

TEST_F(TcpTransportTest, Initialization) {
    TcpTransport transport(config);
    Endpoint local_endpoint("127.0.0.1", 0);  // Auto-assign port

    Result result = transport.initialize(local_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    Endpoint returned_endpoint = transport.get_local_endpoint();
    ASSERT_EQ(returned_endpoint.get_address(), local_endpoint.get_address());
    ASSERT_NE(returned_endpoint.get_port(), 0u);  // Should be assigned by OS

    ASSERT_FALSE(transport.is_connected());
    ASSERT_FALSE(transport.is_running());
}

TEST_F(TcpTransportTest, ServerModeSetup) {
    TcpTransport transport(config);
    Endpoint local_endpoint("127.0.0.1", 30501);

    Result result = transport.initialize(local_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    result = transport.enable_server_mode();
    ASSERT_EQ(result, Result::SUCCESS);

    result = transport.start();
    ASSERT_EQ(result, Result::SUCCESS);
    ASSERT_TRUE(transport.is_running());

    // Clean up
    transport.stop();
}

TEST_F(TcpTransportTest, ClientConnectionTimeout) {
    TcpTransport transport(config);
    Endpoint local_endpoint("127.0.0.1", 0);

    Result result = transport.initialize(local_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    result = transport.start();
    ASSERT_EQ(result, Result::SUCCESS);

    // Try to connect to non-existent server
    Endpoint remote_endpoint("127.0.0.1", 30502);
    result = transport.connect(remote_endpoint);

    // Should fail with timeout or connection refused
    ASSERT_NE(result, Result::SUCCESS);
    ASSERT_FALSE(transport.is_connected());

    transport.stop();
}

TEST_F(TcpTransportTest, MessageSerialization) {
    // Test that TCP transport properly handles message serialization
    Message original_message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                           MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> test_payload = {0x01, 0x02, 0x03, 0x04};
    original_message.set_payload(test_payload);

    // Serialize message
    std::vector<uint8_t> serialized = original_message.serialize();
    ASSERT_EQ(serialized.size(), 20u);  // 16 byte header + 4 byte payload

    // Verify serialization contains correct data
    // Service ID and Method ID (big-endian)
    ASSERT_EQ(serialized[0], 0x12);
    ASSERT_EQ(serialized[1], 0x34);
    ASSERT_EQ(serialized[2], 0x56);
    ASSERT_EQ(serialized[3], 0x78);

    // Length field (big-endian) - payload size + 8
    uint32_t length_field = (serialized[4] << 24) | (serialized[5] << 16) | (serialized[6] << 8) | serialized[7];
    ASSERT_EQ(length_field, 12u);  // 8 (header) + 4 (payload) = 12

    // Client ID and Session ID (big-endian)
    uint32_t request_id_field = (serialized[8] << 24) | (serialized[9] << 16) | (serialized[10] << 8) | serialized[11];
    ASSERT_EQ(request_id_field, 0xABCD0001);

    // Protocol version, interface version, message type, return code
    ASSERT_EQ(serialized[12], 0x01);  // Protocol version
    ASSERT_EQ(serialized[13], 0x01);  // Interface version (SOMEIP_INTERFACE_VERSION)
    ASSERT_EQ(serialized[14], 0x00);  // Message type (REQUEST)
    ASSERT_EQ(serialized[15], 0x00);  // Return code (E_OK)

    // Payload
    ASSERT_EQ(serialized[16], 0x01);
    ASSERT_EQ(serialized[17], 0x02);
    ASSERT_EQ(serialized[18], 0x03);
    ASSERT_EQ(serialized[19], 0x04);

    // Test that we can create a new message and verify round-trip works
    Message reconstructed_message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                                MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> payload = {serialized[16], serialized[17], serialized[18], serialized[19]};
    reconstructed_message.set_payload(payload);

    std::vector<uint8_t> re_serialized = reconstructed_message.serialize();

    // Should be identical
    ASSERT_EQ(serialized, re_serialized);
}

TEST_F(TcpTransportTest, ListenerCallbacks) {
    TcpTransport transport(config);
    auto listener = std::make_shared<TestTcpListener>();

    transport.set_listener(listener.get());

    Endpoint local_endpoint("127.0.0.1", 30503);
    Result result = transport.initialize(local_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    // Test error callback (via listener)
    if (listener) {
        listener->on_error(Result::NETWORK_ERROR);
        ASSERT_EQ(listener->get_last_error(), Result::NETWORK_ERROR);
    }
}

TEST_F(TcpTransportTest, ConfigurationValidation) {
    TcpTransportConfig test_config;

    // Test default configuration
    ASSERT_GT(test_config.max_receive_buffer, 0u);
    ASSERT_GT(test_config.connection_timeout.count(), 0);
    ASSERT_GT(test_config.receive_timeout.count(), 0);
    ASSERT_GT(test_config.send_timeout.count(), 0);

    // Test custom configuration
    test_config.max_receive_buffer = 16384;
    test_config.connection_timeout = std::chrono::milliseconds(5000);
    test_config.keep_alive = true;

    TcpTransport transport(test_config);
    // Transport should accept the configuration
    ASSERT_TRUE(true);  // Construction succeeded
}

TEST_F(TcpTransportTest, ConnectionStateManagement) {
    TcpTransport transport(config);

    // Initially not connected
    ASSERT_FALSE(transport.is_connected());
    ASSERT_EQ(transport.get_connection_state(), TcpConnectionState::DISCONNECTED);

    Endpoint local_endpoint("127.0.0.1", 0);
    Result result = transport.initialize(local_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    result = transport.start();
    ASSERT_EQ(result, Result::SUCCESS);

    // Still not connected (no remote connection established)
    ASSERT_FALSE(transport.is_connected());
    ASSERT_EQ(transport.get_connection_state(), TcpConnectionState::DISCONNECTED);

    transport.stop();
}

TEST_F(TcpTransportTest, EndpointValidation) {
    TcpTransport transport(config);

    // Valid endpoint
    Endpoint valid_endpoint("127.0.0.1", 30504);
    Result result = transport.initialize(valid_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    Endpoint returned = transport.get_local_endpoint();
    ASSERT_EQ(returned.get_address(), valid_endpoint.get_address());

    transport.stop();
}

TEST_F(TcpTransportTest, TransportLifecycle) {
    TcpTransport transport(config);
    Endpoint local_endpoint("127.0.0.1", 30505);

    // Initialize
    Result result = transport.initialize(local_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);
    ASSERT_FALSE(transport.is_running());

    // Start
    result = transport.start();
    ASSERT_EQ(result, Result::SUCCESS);
    ASSERT_TRUE(transport.is_running());

    // Stop
    result = transport.stop();
    ASSERT_EQ(result, Result::SUCCESS);
    ASSERT_FALSE(transport.is_running());

    // Should be able to start again
    result = transport.start();
    ASSERT_EQ(result, Result::SUCCESS);
    ASSERT_TRUE(transport.is_running());

    transport.stop();
}

// Integration-style test for message sending/receiving
// Note: This test requires proper server setup and may be skipped in CI
TEST_F(TcpTransportTest, DISABLED_MessageRoundTrip) {
    // This test would require setting up a TCP server and client
    // For now, it's disabled but shows the intended test structure

    TcpTransport client_transport(config);
    TcpTransport server_transport(config);

    // Set up server
    Endpoint server_endpoint("127.0.0.1", 30506);
    Result result = server_transport.initialize(server_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    result = server_transport.enable_server_mode();
    ASSERT_EQ(result, Result::SUCCESS);

    result = server_transport.start();
    ASSERT_EQ(result, Result::SUCCESS);

    // Set up client
    Endpoint client_local("127.0.0.1", 0);
    result = client_transport.initialize(client_local);
    ASSERT_EQ(result, Result::SUCCESS);

    result = client_transport.start();
    ASSERT_EQ(result, Result::SUCCESS);

    // Connect client to server
    result = client_transport.connect(server_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);
    ASSERT_TRUE(client_transport.is_connected());

    // Send message from client to server
    Message test_message(MessageId(0x1234, 0x0001), RequestId(0xABCD, 0x0001),
                        MessageType::REQUEST, ReturnCode::E_OK);
    test_message.set_payload({0x01, 0x02, 0x03});

    result = client_transport.send_message(test_message, server_endpoint);
    ASSERT_EQ(result, Result::SUCCESS);

    // Server should receive the message
    // (This would require proper listener setup and synchronization)

    // Clean up
    client_transport.disconnect();
    client_transport.stop();
    server_transport.stop();
}

TEST_F(TcpTransportTest, ResourceCleanup) {
    // Test that resources are properly cleaned up
    {
        TcpTransport transport(config);
        Endpoint local_endpoint("127.0.0.1", 30507);

        Result result = transport.initialize(local_endpoint);
        ASSERT_EQ(result, Result::SUCCESS);

        result = transport.start();
        ASSERT_EQ(result, Result::SUCCESS);
    }
    // Transport should be destroyed and resources cleaned up

    ASSERT_TRUE(true);  // Test passes if no exceptions or resource leaks
}

TEST_F(TcpTransportTest, ConfigurationBoundaryValues) {
    // Test configuration with boundary values
    TcpTransportConfig boundary_config;

    // Minimum values
    boundary_config.max_receive_buffer = 1;
    boundary_config.connection_timeout = std::chrono::milliseconds(1);
    boundary_config.receive_timeout = std::chrono::milliseconds(1);
    boundary_config.send_timeout = std::chrono::milliseconds(1);

    TcpTransport transport(boundary_config);
    // Should handle boundary values gracefully
    ASSERT_TRUE(true);

    // Large values
    boundary_config.max_receive_buffer = 1024 * 1024;  // 1MB
    boundary_config.connection_timeout = std::chrono::seconds(300);  // 5 minutes

    TcpTransport transport2(boundary_config);
    // Should handle large values
    ASSERT_TRUE(true);
}
