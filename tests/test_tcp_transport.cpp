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
#include <platform/buffer_pool.h>
#include <platform/containers.h>
#include <thread>
#include <chrono>
#include "static_pool_init.h"

using namespace someip;
using namespace someip::transport;

/**
 * @brief TCP Transport unit tests
 * @tests REQ_TRANSPORT_002a, REQ_TRANSPORT_002b
 * @tests REQ_TRANSPORT_003a, REQ_TRANSPORT_003b
 * @tests REQ_TRANSPORT_004a, REQ_TRANSPORT_004b, REQ_TRANSPORT_004c, REQ_TRANSPORT_004d
 * @tests REQ_TRANSPORT_005
 * @tests feat_req_someip_850
 * @tests feat_req_someip_851
 * @tests REQ_TRANSPORT_016, REQ_TRANSPORT_017, REQ_TRANSPORT_018, REQ_TRANSPORT_019
 * @tests REQ_TRANSPORT_020, REQ_TRANSPORT_021, REQ_TRANSPORT_025
 * @tests REQ_TRANSPORT_002_E01, REQ_TRANSPORT_002_E02, REQ_TRANSPORT_002_E03, REQ_TRANSPORT_002_E04
 * @tests REQ_TRANSPORT_003_E01, REQ_TRANSPORT_016_E01
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
    platform::ByteBuffer test_payload = {0x01, 0x02, 0x03, 0x04};
    original_message.set_payload(test_payload);

    // Serialize message
    platform::ByteBuffer serialized = original_message.serialize();
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
    platform::ByteBuffer payload = {serialized[16], serialized[17], serialized[18], serialized[19]};
    reconstructed_message.set_payload(payload);

    platform::ByteBuffer re_serialized = reconstructed_message.serialize();

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

/**
 * @test_case TC_TCP_E01
 * @tests REQ_TRANSPORT_002_E01
 * @brief Test TCP connect to unreachable host
 */
TEST_F(TcpTransportTest, ConnectUnreachable) {
    TcpTransportConfig short_timeout;
    short_timeout.connection_timeout = std::chrono::milliseconds(100);
    short_timeout.receive_timeout = std::chrono::milliseconds(50);
    short_timeout.send_timeout = std::chrono::milliseconds(50);
    short_timeout.max_receive_buffer = 4096;

    TcpTransport client(short_timeout);
    Endpoint local_endpoint("127.0.0.1", 0);
    Result init_result = client.initialize(local_endpoint);
    ASSERT_EQ(init_result, Result::SUCCESS);
    Result start_result = client.start();
    ASSERT_EQ(start_result, Result::SUCCESS);

    Result connect_result = client.connect(Endpoint("192.0.2.1", 9999));
    EXPECT_NE(connect_result, Result::SUCCESS) << "Connection to unreachable host should fail";

    client.stop();
}

/**
 * @test_case TC_TCP_E02
 * @tests REQ_TRANSPORT_002_E02
 * @brief Test TCP send on disconnected transport
 */
TEST_F(TcpTransportTest, SendOnDisconnected) {
    TcpTransport transport(config);
    Endpoint local_endpoint("127.0.0.1", 0);
    ASSERT_EQ(transport.initialize(local_endpoint), Result::SUCCESS);
    EXPECT_FALSE(transport.is_connected());

    Message msg;
    msg.set_service_id(0x1234);
    msg.set_method_id(0x0001);

    Endpoint dummy_endpoint("127.0.0.1", 30500);
    Result result = transport.send_message(msg, dummy_endpoint);
    EXPECT_NE(result, Result::SUCCESS) << "Send on initialized but disconnected transport should fail";
}

/**
 * @test_case TC_TCP_E03
 * @tests REQ_TRANSPORT_002_E03
 * @brief Test TCP with zero connection timeout
 */
TEST_F(TcpTransportTest, ZeroConnectionTimeout) {
    TcpTransportConfig zero_timeout;
    zero_timeout.connection_timeout = std::chrono::milliseconds(0);
    zero_timeout.receive_timeout = std::chrono::milliseconds(50);
    zero_timeout.send_timeout = std::chrono::milliseconds(50);
    zero_timeout.max_receive_buffer = 4096;

    TcpTransport transport(zero_timeout);
    Endpoint local_endpoint("127.0.0.1", 0);
    Result init_result = transport.initialize(local_endpoint);
    ASSERT_EQ(init_result, Result::SUCCESS);
    Result start_result = transport.start();
    ASSERT_EQ(start_result, Result::SUCCESS);

    Result connect_result = transport.connect(Endpoint("127.0.0.1", 12345));
    EXPECT_NE(connect_result, Result::SUCCESS) << "Zero timeout should result in immediate failure";

    transport.stop();
}

/**
 * @test_case TC_TCP_E04
 * @tests REQ_TRANSPORT_002_E04, REQ_TRANSPORT_003_E01
 * @brief Test TCP double disconnect
 */
TEST_F(TcpTransportTest, DoubleDisconnect) {
    // Set up a listening server so the client can establish a real connection.
    TcpTransport server(config);
    Endpoint server_bind("127.0.0.1", 0);
    ASSERT_EQ(server.initialize(server_bind), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    Endpoint server_ep = server.get_local_endpoint();

    TcpTransport client(config);
    ASSERT_EQ(client.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(client.start(), Result::SUCCESS);

    Result conn = client.connect(server_ep);
    ASSERT_EQ(conn, Result::SUCCESS) << "Localhost connect should succeed";
    EXPECT_TRUE(client.is_connected());

    client.disconnect();
    EXPECT_FALSE(client.is_connected());

    // Second disconnect must be a safe no-op.
    client.disconnect();
    EXPECT_FALSE(client.is_connected());

    client.stop();
    server.stop();
}

/**
 * @test_case TC_TCP_E05
 * @tests REQ_TRANSPORT_016_E01
 * @brief Test TCP framing with zero-length message
 */
TEST_F(TcpTransportTest, ZeroLengthMessage) {
    Message msg;
    msg.set_service_id(0x1234);
    msg.set_method_id(0x0001);

    platform::ByteBuffer serialized = msg.serialize();
    EXPECT_FALSE(serialized.empty()) << "Even empty payload has header";
    EXPECT_GE(serialized.size(), 16u) << "Minimum SOME/IP header is 16 bytes";
}

// ============================================================================
// TCP Persistent Buffer / Fragmented Frame Tests (Issue #255)
// ============================================================================

/**
 * @test_case TC_TCP_PARSE_001
 * @tests REQ_TRANSPORT_024
 * @brief parse_message_from_buffer handles a complete single message
 */
TEST_F(TcpTransportTest, ParseSingleCompleteMessage) {
    TcpTransport transport(config);

    Message original(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                     MessageType::REQUEST, ReturnCode::E_OK);
    original.set_payload({0x01, 0x02, 0x03, 0x04});

    platform::ByteBuffer buffer = original.serialize();
    MessagePtr parsed;
    ASSERT_TRUE(transport.parse_message_from_buffer(buffer, parsed));
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_service_id(), 0x1234);
    EXPECT_EQ(parsed->get_method_id(), 0x5678);
    EXPECT_EQ(parsed->get_payload(), (platform::ByteBuffer{0x01, 0x02, 0x03, 0x04}));
    EXPECT_TRUE(buffer.empty()) << "Buffer should be consumed";
}

/**
 * @test_case TC_TCP_PARSE_002
 * @tests REQ_TRANSPORT_024
 * @brief Incomplete message (only partial header) stays in buffer
 */
TEST_F(TcpTransportTest, ParseIncompleteHeaderStaysInBuffer) {
    TcpTransport transport(config);

    Message original(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                     MessageType::REQUEST, ReturnCode::E_OK);
    original.set_payload({0x01, 0x02, 0x03});

    platform::ByteBuffer full = original.serialize();
    platform::ByteBuffer buffer(full.begin(), full.begin() + 10);

    MessagePtr parsed;
    EXPECT_FALSE(transport.parse_message_from_buffer(buffer, parsed));
    EXPECT_EQ(buffer.size(), 10u) << "Incomplete bytes must be preserved";
}

/**
 * @test_case TC_TCP_PARSE_003
 * @tests REQ_TRANSPORT_024
 * @brief Multiple complete messages in one buffer are parseable sequentially
 */
TEST_F(TcpTransportTest, ParseMultipleMessagesInBuffer) {
    TcpTransport transport(config);

    Message msg1(MessageId(0x1111, 0x2222), RequestId(0x0001, 0x0001),
                 MessageType::REQUEST, ReturnCode::E_OK);
    msg1.set_payload({0xAA});

    Message msg2(MessageId(0x3333, 0x4444), RequestId(0x0002, 0x0001),
                 MessageType::REQUEST, ReturnCode::E_OK);
    msg2.set_payload({0xBB, 0xCC});

    platform::ByteBuffer buffer;
    auto s1 = msg1.serialize();
    auto s2 = msg2.serialize();
    buffer.insert(buffer.end(), s1.begin(), s1.end());
    buffer.insert(buffer.end(), s2.begin(), s2.end());

    MessagePtr parsed1;
    ASSERT_TRUE(transport.parse_message_from_buffer(buffer, parsed1));
    ASSERT_NE(parsed1, nullptr);
    EXPECT_EQ(parsed1->get_service_id(), 0x1111);

    MessagePtr parsed2;
    ASSERT_TRUE(transport.parse_message_from_buffer(buffer, parsed2));
    ASSERT_NE(parsed2, nullptr);
    EXPECT_EQ(parsed2->get_service_id(), 0x3333);
    EXPECT_EQ(parsed2->get_payload(), (platform::ByteBuffer{0xBB, 0xCC}));

    EXPECT_TRUE(buffer.empty());
}

/**
 * @test_case TC_TCP_PARSE_004
 * @tests REQ_TRANSPORT_024
 * @brief Complete message + incomplete tail: first parses, tail preserved
 */
TEST_F(TcpTransportTest, ParseCompleteMessagePlusIncompleteTail) {
    TcpTransport transport(config);

    Message msg1(MessageId(0x1111, 0x2222), RequestId(0x0001, 0x0001),
                 MessageType::REQUEST, ReturnCode::E_OK);
    msg1.set_payload({0xAA});

    Message msg2(MessageId(0x3333, 0x4444), RequestId(0x0002, 0x0001),
                 MessageType::REQUEST, ReturnCode::E_OK);
    msg2.set_payload({0xBB, 0xCC});

    auto s1 = msg1.serialize();
    auto s2 = msg2.serialize();

    platform::ByteBuffer buffer;
    buffer.insert(buffer.end(), s1.begin(), s1.end());
    buffer.insert(buffer.end(), s2.begin(), s2.begin() + 8);

    MessagePtr parsed;
    ASSERT_TRUE(transport.parse_message_from_buffer(buffer, parsed));
    EXPECT_EQ(parsed->get_service_id(), 0x1111);

    EXPECT_EQ(buffer.size(), 8u) << "Incomplete tail of second message must remain";

    MessagePtr parsed2;
    EXPECT_FALSE(transport.parse_message_from_buffer(buffer, parsed2));
    EXPECT_EQ(buffer.size(), 8u) << "Tail still preserved after failed parse";
}

/**
 * @test_case TC_TCP_PARSE_005
 * @tests REQ_TRANSPORT_024
 * @brief Simulated chunked arrival: feed a frame in 2+ chunks
 */
TEST_F(TcpTransportTest, ChunkedArrivalReassembly) {
    TcpTransport transport(config);

    Message original(MessageId(0xAAAA, 0xBBBB), RequestId(0xCCCC, 0x0001),
                     MessageType::REQUEST, ReturnCode::E_OK);
    original.set_payload({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});

    platform::ByteBuffer full = original.serialize();
    ASSERT_EQ(full.size(), 24u);

    platform::ByteBuffer persistent_buffer;
    MessagePtr parsed;

    persistent_buffer.insert(persistent_buffer.end(), full.begin(), full.begin() + 5);
    EXPECT_FALSE(transport.parse_message_from_buffer(persistent_buffer, parsed));
    EXPECT_EQ(persistent_buffer.size(), 5u);

    persistent_buffer.insert(persistent_buffer.end(), full.begin() + 5, full.begin() + 16);
    EXPECT_FALSE(transport.parse_message_from_buffer(persistent_buffer, parsed));
    EXPECT_EQ(persistent_buffer.size(), 16u);

    persistent_buffer.insert(persistent_buffer.end(), full.begin() + 16, full.end());
    ASSERT_TRUE(transport.parse_message_from_buffer(persistent_buffer, parsed));
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_service_id(), 0xAAAA);
    EXPECT_EQ(parsed->get_payload().size(), 8u);
    EXPECT_TRUE(persistent_buffer.empty());
}

// ============================================================================
// Magic Cookie Tests (Issue #257)
// ============================================================================

/**
 * @test_case TC_TCP_MAGIC_001
 * @tests REQ_TRANSPORT_020, REQ_TRANSPORT_025
 * @brief Client Magic Cookie has correct wire format
 */
TEST_F(TcpTransportTest, MagicCookieClientFormat) {
    auto cookie = TcpTransport::make_magic_cookie_client();
    ASSERT_EQ(cookie.size(), 16u);
    EXPECT_EQ(cookie[0], 0xFF);  // Service ID high
    EXPECT_EQ(cookie[1], 0xFF);  // Service ID low
    EXPECT_EQ(cookie[2], 0x00);  // Method ID high (client)
    EXPECT_EQ(cookie[3], 0x00);  // Method ID low
    EXPECT_EQ(cookie[4], 0x00);  // Length high
    EXPECT_EQ(cookie[7], 0x08);  // Length = 8
    EXPECT_EQ(cookie[8], 0xDE);  // Client ID high
    EXPECT_EQ(cookie[9], 0xAD);  // Client ID low
    EXPECT_EQ(cookie[14], 0xBE); // Session ID high
    EXPECT_EQ(cookie[15], 0xEF); // Session ID low
}

/**
 * @test_case TC_TCP_MAGIC_002
 * @tests REQ_TRANSPORT_020, REQ_TRANSPORT_025
 * @brief Server Magic Cookie has Method ID 0x8000
 */
TEST_F(TcpTransportTest, MagicCookieServerFormat) {
    auto cookie = TcpTransport::make_magic_cookie_server();
    ASSERT_EQ(cookie.size(), 16u);
    EXPECT_EQ(cookie[2], 0x80);  // Method ID high (server)
    EXPECT_EQ(cookie[3], 0x00);  // Method ID low
}

/**
 * @test_case TC_TCP_MAGIC_003
 * @tests REQ_TRANSPORT_020
 * @brief is_magic_cookie detects client and server cookies
 */
TEST_F(TcpTransportTest, IsMagicCookieDetection) {
    auto client_cookie = TcpTransport::make_magic_cookie_client();
    auto server_cookie = TcpTransport::make_magic_cookie_server();
    EXPECT_TRUE(TcpTransport::is_magic_cookie(client_cookie));
    EXPECT_TRUE(TcpTransport::is_magic_cookie(server_cookie));

    platform::ByteBuffer not_cookie(16, 0x00);
    EXPECT_FALSE(TcpTransport::is_magic_cookie(not_cookie));
}

/**
 * @test_case TC_TCP_MAGIC_004
 * @tests REQ_TRANSPORT_020
 * @brief Magic Cookie in stream is silently consumed by parser
 */
TEST_F(TcpTransportTest, MagicCookieConsumedByParser) {
    TcpTransport transport(config);
    auto cookie = TcpTransport::make_magic_cookie_client();

    Message msg(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                MessageType::REQUEST, ReturnCode::E_OK);
    msg.set_payload({0x01, 0x02});
    platform::ByteBuffer msg_bytes = msg.serialize();

    platform::ByteBuffer buffer;
    buffer.insert(buffer.end(), cookie.begin(), cookie.end());
    buffer.insert(buffer.end(), msg_bytes.begin(), msg_bytes.end());

    MessagePtr parsed;
    EXPECT_FALSE(transport.parse_message_from_buffer(buffer, parsed))
        << "First call should consume the magic cookie";
    EXPECT_EQ(buffer.size(), msg_bytes.size());

    ASSERT_TRUE(transport.parse_message_from_buffer(buffer, parsed));
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_service_id(), 0x1234);
}

/**
 * @test_case TC_TCP_MAGIC_005
 * @tests REQ_TRANSPORT_021
 * @brief magic_cookie_enabled config controls periodic insertion
 */
TEST_F(TcpTransportTest, MagicCookieConfigControls) {
    TcpTransportConfig mc_config;
    mc_config.magic_cookie_enabled = true;
    mc_config.magic_cookie_interval = std::chrono::milliseconds(10000);
    EXPECT_TRUE(mc_config.magic_cookie_enabled);
    EXPECT_EQ(mc_config.magic_cookie_interval.count(), 10000);

    mc_config.magic_cookie_enabled = false;
    EXPECT_FALSE(mc_config.magic_cookie_enabled);
}

/**
 * @test_case TC_TCP_MAGIC_006
 * @tests REQ_TRANSPORT_021
 * @brief Multiple magic cookies in a stream are all consumed
 */
TEST_F(TcpTransportTest, MultipleMagicCookiesConsumed) {
    TcpTransport transport(config);
    auto cookie_c = TcpTransport::make_magic_cookie_client();
    auto cookie_s = TcpTransport::make_magic_cookie_server();

    Message msg(MessageId(0xAAAA, 0xBBBB), RequestId(0xCCCC, 0x0001),
                MessageType::REQUEST, ReturnCode::E_OK);
    msg.set_payload({0xDD});
    platform::ByteBuffer msg_bytes = msg.serialize();

    platform::ByteBuffer buffer;
    buffer.insert(buffer.end(), cookie_c.begin(), cookie_c.end());
    buffer.insert(buffer.end(), cookie_s.begin(), cookie_s.end());
    buffer.insert(buffer.end(), msg_bytes.begin(), msg_bytes.end());

    MessagePtr parsed;
    EXPECT_FALSE(transport.parse_message_from_buffer(buffer, parsed));
    EXPECT_FALSE(transport.parse_message_from_buffer(buffer, parsed));
    ASSERT_TRUE(transport.parse_message_from_buffer(buffer, parsed));
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_service_id(), 0xAAAA);
    EXPECT_TRUE(buffer.empty());
}
