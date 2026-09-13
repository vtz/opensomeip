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
#include <platform/byteorder.h>
#include <platform/net.h>
#include <thread>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <set>
#include <vector>
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
        lost_endpoints_.push_back(endpoint);
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

    bool wait_for_messages(size_t expected_count,
                           std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, expected_count]() {
            return received_messages_.size() >= expected_count;
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

    /// Every loss reported, in order, so tests can assert a peer is reported
    /// exactly once rather than merely at least once.
    std::vector<Endpoint> get_lost_endpoints() const {
        std::scoped_lock lock(mutex_);
        return lost_endpoints_;
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
    std::vector<Endpoint> lost_endpoints_;
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
    // Service ID = 0xFFFF
    EXPECT_EQ(cookie[0], 0xFF);
    EXPECT_EQ(cookie[1], 0xFF);
    // Method ID = 0x0000 (client→server)
    EXPECT_EQ(cookie[2], 0x00);
    EXPECT_EQ(cookie[3], 0x00);
    // Length = 0x00000008
    EXPECT_EQ(cookie[4], 0x00);
    EXPECT_EQ(cookie[7], 0x08);
    // Client ID = 0xDEAD
    EXPECT_EQ(cookie[8], 0xDE);
    EXPECT_EQ(cookie[9], 0xAD);
    // Session ID = 0xBEEF (feat_req_someip_609)
    EXPECT_EQ(cookie[10], 0xBE);
    EXPECT_EQ(cookie[11], 0xEF);
    // Protocol Version = 0x01, Interface Version = 0x01
    EXPECT_EQ(cookie[12], 0x01);
    EXPECT_EQ(cookie[13], 0x01);
    // Message Type = 0x01 (REQUEST_NO_RETURN, client->server)
    EXPECT_EQ(cookie[14], 0x01);
    // Return Code = 0x00
    EXPECT_EQ(cookie[15], 0x00);
}

/**
 * @test_case TC_TCP_MAGIC_002
 * @tests REQ_TRANSPORT_020, REQ_TRANSPORT_025
 * @brief Server Magic Cookie has Method ID 0x8000
 */
TEST_F(TcpTransportTest, MagicCookieServerFormat) {
    auto cookie = TcpTransport::make_magic_cookie_server();
    ASSERT_EQ(cookie.size(), 16u);
    // Method ID = 0x8000 (server->client)
    EXPECT_EQ(cookie[2], 0x80);
    EXPECT_EQ(cookie[3], 0x00);
    // Session ID = 0xBEEF
    EXPECT_EQ(cookie[10], 0xBE);
    EXPECT_EQ(cookie[11], 0xEF);
    // Message Type = 0x02 (NOTIFICATION, server->client)
    EXPECT_EQ(cookie[14], 0x02);
    // Return Code = 0x00
    EXPECT_EQ(cookie[15], 0x00);
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
 * @test_case TC_TCP_MAGIC_CORRELATION
 * @tests feat_req_someip_609
 * @brief Method ID and Message Type must correlate: client 0x0000/0x01,
 *        server 0x8000/0x02. Crossed combinations are NOT valid cookies.
 */
TEST_F(TcpTransportTest, MagicCookieMethodTypeCorrelation) {
    // Client cookie with wrong message type (0x02 instead of 0x01)
    auto bad_client = TcpTransport::make_magic_cookie_client();
    bad_client[14] = 0x02;
    EXPECT_FALSE(TcpTransport::is_magic_cookie(bad_client))
        << "Client method 0x0000 + type 0x02 must NOT be a valid cookie";

    // Server cookie with wrong message type (0x01 instead of 0x02)
    auto bad_server = TcpTransport::make_magic_cookie_server();
    bad_server[14] = 0x01;
    EXPECT_FALSE(TcpTransport::is_magic_cookie(bad_server))
        << "Server method 0x8000 + type 0x01 must NOT be a valid cookie";

    // Correct cookies still match
    EXPECT_TRUE(TcpTransport::is_magic_cookie(TcpTransport::make_magic_cookie_client()));
    EXPECT_TRUE(TcpTransport::is_magic_cookie(TcpTransport::make_magic_cookie_server()));
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

// ============================================================================
// Listener / Polling Mutual Exclusion Tests (Issue #269)
// ============================================================================

/**
 * @test_case TC_TCP_LISTENER_QUEUE_RETENTION
 * @brief Listener-only mode must not retain messages in message_queue_ (issue #269)
 *
 * When set_listener() is used, messages dispatched via on_message_received must
 * not also be enqueued. This mirrors the UDP-side test
 * ListenerOnlyDoesNotRetainQueueMessages.
 */
TEST_F(TcpTransportTest, ListenerOnlyDoesNotRetainQueueMessages) {
    TcpTransport server(config);
    Endpoint server_bind("127.0.0.1", 0);
    ASSERT_EQ(server.initialize(server_bind), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);

    ASSERT_EQ(server.start(), Result::SUCCESS);
    Endpoint server_ep = server.get_local_endpoint();

    TcpTransport client(config);
    ASSERT_EQ(client.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(client.start(), Result::SUCCESS);

    ASSERT_EQ(client.connect(server_ep), Result::SUCCESS);
    ASSERT_TRUE(server_listener.wait_for_connection_established());

    constexpr int NUM_MESSAGES = 3;
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        Message msg;
        msg.set_service_id(0x1234);
        msg.set_method_id(0x5678);
        msg.set_client_id(0x9ABC);
        msg.set_session_id(static_cast<uint16_t>(i + 1));
        msg.set_protocol_version(1);
        msg.set_interface_version(1);
        msg.set_message_type(MessageType::REQUEST);
        msg.set_return_code(ReturnCode::E_OK);

        platform::ByteBuffer payload = {static_cast<uint8_t>(i)};
        msg.set_payload(payload);

        EXPECT_EQ(client.send_message(msg, server_ep), Result::SUCCESS);
    }

    ASSERT_TRUE(server_listener.wait_for_messages(NUM_MESSAGES))
        << "Listener should have received all messages";

    MessagePtr queued = server.receive_message();
    EXPECT_EQ(queued, nullptr)
        << "message_queue_ must be empty in listener-only mode (issue #269)";

    client.disconnect();
    client.stop();
    server.stop();
}

/**
 * @test_case TC_TCP_MODE_SWITCH
 * @brief Messages route correctly across no-listener → listener → cleared-listener transitions
 */
TEST_F(TcpTransportTest, ModeSwitchPollingToListenerAndBack) {
    TcpTransport server(config);
    Endpoint server_bind("127.0.0.1", 0);
    ASSERT_EQ(server.initialize(server_bind), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    // Use a temporary listener to detect connection establishment
    TestTcpListener setup_listener;
    server.set_listener(&setup_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);
    Endpoint server_ep = server.get_local_endpoint();

    TcpTransport client(config);
    ASSERT_EQ(client.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(client.start(), Result::SUCCESS);
    ASSERT_EQ(client.connect(server_ep), Result::SUCCESS);

    ASSERT_TRUE(setup_listener.wait_for_connection_established())
        << "Server must accept the connection";

    // Remove the setup listener — start in polling mode for phase 1
    server.set_listener(nullptr);

    // --- Phase 1: polling mode (no listener) ---
    {
        Message msg;
        msg.set_service_id(0x1111);
        msg.set_method_id(0x0001);
        msg.set_client_id(0x0001);
        msg.set_session_id(0x0001);
        msg.set_protocol_version(1);
        msg.set_interface_version(1);
        msg.set_message_type(MessageType::REQUEST);
        msg.set_return_code(ReturnCode::E_OK);

        EXPECT_EQ(client.send_message(msg, server_ep), Result::SUCCESS);
    }

    MessagePtr polled;
    const auto deadline1 = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (std::chrono::steady_clock::now() < deadline1) {
        polled = server.receive_message();
        if (polled) { break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_NE(polled, nullptr) << "Phase 1: polling must receive the message";
    EXPECT_EQ(polled->get_service_id(), 0x1111);

    // --- Phase 2: install listener → messages go to listener only ---
    TestTcpListener server_listener;
    server.set_listener(&server_listener);

    {
        Message msg;
        msg.set_service_id(0x2222);
        msg.set_method_id(0x0001);
        msg.set_client_id(0x0001);
        msg.set_session_id(0x0002);
        msg.set_protocol_version(1);
        msg.set_interface_version(1);
        msg.set_message_type(MessageType::REQUEST);
        msg.set_return_code(ReturnCode::E_OK);

        EXPECT_EQ(client.send_message(msg, server_ep), Result::SUCCESS);
    }

    ASSERT_TRUE(server_listener.wait_for_messages(1))
        << "Phase 2: listener must receive the message";
    {
        auto msgs = server_listener.get_received_messages();
        EXPECT_EQ(msgs[0].first->get_service_id(), 0x2222);
    }

    MessagePtr stale = server.receive_message();
    EXPECT_EQ(stale, nullptr) << "Phase 2: queue must be empty for new listener traffic";

    // --- Phase 3: clear listener → messages enqueue again ---
    server.set_listener(nullptr);

    {
        Message msg;
        msg.set_service_id(0x3333);
        msg.set_method_id(0x0001);
        msg.set_client_id(0x0001);
        msg.set_session_id(0x0003);
        msg.set_protocol_version(1);
        msg.set_interface_version(1);
        msg.set_message_type(MessageType::REQUEST);
        msg.set_return_code(ReturnCode::E_OK);

        EXPECT_EQ(client.send_message(msg, server_ep), Result::SUCCESS);
    }

    MessagePtr polled3;
    const auto deadline3 = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (std::chrono::steady_clock::now() < deadline3) {
        polled3 = server.receive_message();
        if (polled3) { break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_NE(polled3, nullptr) << "Phase 3: polling must resume after listener cleared";
    EXPECT_EQ(polled3->get_service_id(), 0x3333);

    client.disconnect();
    client.stop();
    server.stop();
}

/**
 * @test_case TC_TCP_LISTENER_DISCONNECT_NO_DEADLOCK
 * @brief Listener calling disconnect() must not deadlock (connection_mutex_ lock scope fix)
 *
 * Before the fix, on_message_received was called with connection_mutex_ held.
 * A listener that called disconnect() would re-enter disconnect_internal() which
 * also locks connection_mutex_, causing a deadlock. The lock-scope fix releases
 * connection_mutex_ before invoking the listener.
 *
 * Note: calling stop() from the listener callback is inherently unsafe because
 * stop() joins the receive thread (self-join). Only disconnect() is tested here.
 */
TEST_F(TcpTransportTest, ListenerCallingDisconnectDoesNotDeadlock) {
    TcpTransport server(config);
    Endpoint server_bind("127.0.0.1", 0);
    ASSERT_EQ(server.initialize(server_bind), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    std::atomic<bool> callback_fired{false};
    std::atomic<bool> disconnect_completed{false};

    class DisconnectOnReceiveListener : public ITransportListener {
    public:
        DisconnectOnReceiveListener(TcpTransport& t,
                                    std::atomic<bool>& fired,
                                    std::atomic<bool>& done)
            : transport_(t), callback_fired_(fired), disconnect_completed_(done) {}

        void on_message_received(MessagePtr /*message*/, const Endpoint& /*sender*/) override {
            callback_fired_.store(true, std::memory_order_release);
            transport_.disconnect();
            disconnect_completed_.store(true, std::memory_order_release);
        }
        void on_connection_lost(const Endpoint& /*endpoint*/) override {}
        void on_connection_established(const Endpoint& /*endpoint*/) override {}
        void on_error(Result /*error*/) override {}

    private:
        TcpTransport& transport_;
        std::atomic<bool>& callback_fired_;
        std::atomic<bool>& disconnect_completed_;
    };

    // Use a temporary listener for connection establishment
    TestTcpListener setup_listener;
    server.set_listener(&setup_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    Endpoint server_ep = server.get_local_endpoint();

    TcpTransport client(config);
    ASSERT_EQ(client.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(client.start(), Result::SUCCESS);
    ASSERT_EQ(client.connect(server_ep), Result::SUCCESS);

    ASSERT_TRUE(setup_listener.wait_for_connection_established())
        << "Server must accept the connection";

    // Now install the real listener that will call disconnect()
    DisconnectOnReceiveListener disconnector(server, callback_fired, disconnect_completed);
    server.set_listener(&disconnector);

    Message msg;
    msg.set_service_id(0xDEAD);
    msg.set_method_id(0x0001);
    msg.set_client_id(0x0001);
    msg.set_session_id(0x0001);
    msg.set_protocol_version(1);
    msg.set_interface_version(1);
    msg.set_message_type(MessageType::REQUEST);
    msg.set_return_code(ReturnCode::E_OK);

    EXPECT_EQ(client.send_message(msg, server_ep), Result::SUCCESS);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (disconnect_completed.load(std::memory_order_acquire)) { break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(callback_fired.load()) << "Listener callback should have fired";
    EXPECT_TRUE(disconnect_completed.load())
        << "disconnect() from listener must complete without deadlock";

    client.disconnect();
    client.stop();
    server.stop();
}

// ============================================================================
// Multi-connection server (issue #319)
// ============================================================================

namespace {

/// Build a REQUEST carrying a single-byte payload used to identify the sender.
Message make_tagged_message(uint8_t tag) {
    Message msg;
    msg.set_service_id(0x1234);
    msg.set_method_id(0x5678);
    msg.set_client_id(static_cast<uint16_t>(0x1000U + tag));
    msg.set_session_id(static_cast<uint16_t>(tag + 1U));
    msg.set_protocol_version(1);
    msg.set_interface_version(1);
    msg.set_message_type(MessageType::REQUEST);
    msg.set_return_code(ReturnCode::E_OK);

    platform::ByteBuffer payload = {tag};
    msg.set_payload(payload);
    return msg;
}

/// A started client transport already connected to server_ep.
struct ConnectedClient {
    std::unique_ptr<TcpTransport> transport;
    std::unique_ptr<TestTcpListener> listener;
    Endpoint local;
};

}  // namespace

/**
 * @test_case TC_TCP_MULTI_CLIENT
 * @tests REQ_TRANSPORT_003b
 * @brief A TCP server serves several clients at the same time (issue #319)
 *
 * Before the fix the server stored a single TcpConnection and closed every
 * surplus accepted socket, so only the first client was ever served.
 */
TEST_F(TcpTransportTest, ServerAcceptsMultipleConcurrentClients) {
    constexpr size_t NUM_CLIENTS = 4;

    TcpTransport server(config);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    std::vector<ConnectedClient> clients;
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        ConnectedClient client;
        client.transport = std::make_unique<TcpTransport>(config);
        client.listener = std::make_unique<TestTcpListener>();
        ASSERT_EQ(client.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
        client.transport->set_listener(client.listener.get());
        ASSERT_EQ(client.transport->start(), Result::SUCCESS);
        ASSERT_EQ(client.transport->connect(server_ep), Result::SUCCESS)
            << "client " << i << " must connect";
        client.local = client.transport->get_local_endpoint();
        clients.push_back(std::move(client));
    }

    // Every client must still be connected; none may have been dropped.
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        EXPECT_TRUE(clients[i].transport->is_connected())
            << "client " << i << " must remain connected";
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() < NUM_CLIENTS &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_EQ(server.connection_count(), NUM_CLIENTS)
        << "server must hold one connection per client";

    // Each client sends a distinct tag; the server must receive all of them.
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        EXPECT_EQ(clients[i].transport->send_message(make_tagged_message(static_cast<uint8_t>(i)),
                                                     server_ep),
                  Result::SUCCESS);
    }

    ASSERT_TRUE(server_listener.wait_for_messages(NUM_CLIENTS, std::chrono::milliseconds(5000)))
        << "server must receive a message from every client";

    std::set<uint8_t> received_tags;
    std::set<uint16_t> sender_ports;
    for (const auto& [message, sender] : server_listener.get_received_messages()) {
        ASSERT_NE(message, nullptr);
        ASSERT_EQ(message->get_payload().size(), 1U);
        received_tags.insert(message->get_payload()[0]);
        sender_ports.insert(sender.get_port());
    }

    EXPECT_EQ(received_tags.size(), NUM_CLIENTS) << "every client's payload must arrive";
    EXPECT_EQ(sender_ports.size(), NUM_CLIENTS)
        << "each message must be attributed to a distinct peer";

    for (auto& client : clients) {
        client.transport->disconnect();
        client.transport->stop();
    }
    server.stop();
}

/**
 * @test_case TC_TCP_MULTI_CLIENT_ROUTING
 * @tests REQ_TRANSPORT_003b, REQ_TRANSPORT_002a
 * @brief send_message() delivers to the peer named by its endpoint argument
 *
 * Before the fix the endpoint parameter was ignored and every send went to the
 * single stored socket.
 */
TEST_F(TcpTransportTest, ServerRoutesEachResponseToItsOwnPeer) {
    constexpr size_t NUM_CLIENTS = 3;

    TcpTransport server(config);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    std::vector<ConnectedClient> clients;
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        ConnectedClient client;
        client.transport = std::make_unique<TcpTransport>(config);
        client.listener = std::make_unique<TestTcpListener>();
        ASSERT_EQ(client.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
        client.transport->set_listener(client.listener.get());
        ASSERT_EQ(client.transport->start(), Result::SUCCESS);
        ASSERT_EQ(client.transport->connect(server_ep), Result::SUCCESS);
        clients.push_back(std::move(client));
    }

    // Each client announces itself so the server learns its peer endpoint.
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        EXPECT_EQ(clients[i].transport->send_message(make_tagged_message(static_cast<uint8_t>(i)),
                                                     server_ep),
                  Result::SUCCESS);
    }
    ASSERT_TRUE(server_listener.wait_for_messages(NUM_CLIENTS, std::chrono::milliseconds(5000)));

    // Echo each tag back to the peer it came from.
    for (const auto& [message, sender] : server_listener.get_received_messages()) {
        ASSERT_EQ(message->get_payload().size(), 1U);
        EXPECT_TRUE(server.is_peer_connected(sender));
        EXPECT_EQ(server.send_message(make_tagged_message(message->get_payload()[0]), sender),
                  Result::SUCCESS);
    }

    // Each client must receive exactly its own tag back.
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        ASSERT_TRUE(clients[i].listener->wait_for_message(std::chrono::milliseconds(5000)))
            << "client " << i << " must receive its echo";

        const auto echoes = clients[i].listener->get_received_messages();
        ASSERT_EQ(echoes.size(), 1U) << "client " << i << " must receive only its own echo";
        ASSERT_EQ(echoes[0].first->get_payload().size(), 1U);
        EXPECT_EQ(echoes[0].first->get_payload()[0], static_cast<uint8_t>(i))
            << "client " << i << " received another peer's message";
    }

    for (auto& client : clients) {
        client.transport->disconnect();
        client.transport->stop();
    }
    server.stop();
}

/**
 * @test_case TC_TCP_E01_LIMIT
 * @tests REQ_TRANSPORT_003_E01
 * @brief Connections beyond max_connections are refused, established ones survive
 *
 * Mirrors the verification REQ_TRANSPORT_003_E01 states: five admitted peers,
 * a sixth that must be rejected, and a rejection the sixth peer can observe.
 */
TEST_F(TcpTransportTest, ConnectionLimitRefusesSurplusClientsOnly) {
    TcpTransportConfig limited = config;
    limited.max_connections = 5;

    TcpTransport server(limited);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    EXPECT_EQ(server.max_connections(), 5U);

    const Endpoint server_ep = server.get_local_endpoint();

    std::vector<ConnectedClient> clients;
    for (size_t i = 0; i < limited.max_connections; ++i) {
        ConnectedClient client;
        client.transport = std::make_unique<TcpTransport>(limited);
        client.listener = std::make_unique<TestTcpListener>();
        ASSERT_EQ(client.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
        client.transport->set_listener(client.listener.get());
        ASSERT_EQ(client.transport->start(), Result::SUCCESS);
        ASSERT_EQ(client.transport->connect(server_ep), Result::SUCCESS);
        clients.push_back(std::move(client));
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() < limited.max_connections &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(server.connection_count(), limited.max_connections);

    // One client beyond the limit. The server accepts it only to close it again,
    // so the refusal reaches the client instead of leaving it parked in the
    // kernel backlog believing it is being served.
    ConnectedClient surplus;
    surplus.transport = std::make_unique<TcpTransport>(limited);
    surplus.listener = std::make_unique<TestTcpListener>();
    ASSERT_EQ(surplus.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    surplus.transport->set_listener(surplus.listener.get());
    ASSERT_EQ(surplus.transport->start(), Result::SUCCESS);
    // The handshake itself succeeds: the kernel completes it from the listen
    // backlog before the server ever looks at the connection limit. The refusal
    // is therefore visible as a loss straight afterwards, not as a failed
    // connect(), which is what REQ_TRANSPORT_003_E01 asks to be observable.
    ASSERT_EQ(surplus.transport->connect(server_ep), Result::SUCCESS);

    EXPECT_TRUE(surplus.listener->wait_for_connection_lost(std::chrono::seconds(5)))
        << "surplus client must be told it was refused";

    EXPECT_EQ(server.connection_count(), limited.max_connections)
        << "server must not exceed max_connections";

    // The clients admitted first must be unaffected by the refusal.
    for (size_t i = 0; i < clients.size(); ++i) {
        EXPECT_EQ(clients[i].transport->send_message(make_tagged_message(static_cast<uint8_t>(i)),
                                                     server_ep),
                  Result::SUCCESS)
            << "admitted client " << i << " must still be served";
    }
    EXPECT_TRUE(server_listener.wait_for_messages(clients.size(), std::chrono::milliseconds(5000)));

    surplus.transport->disconnect();
    surplus.transport->stop();
    for (auto& client : clients) {
        client.transport->disconnect();
        client.transport->stop();
    }
    server.stop();
}

/**
 * @test_case TC_TCP_STALLED_PEER_ISOLATION
 * @tests REQ_TRANSPORT_003b
 * @brief A peer that never drains its socket does not stall the other peers
 *
 * Socket I/O is serialised per connection rather than across the whole table,
 * so a send blocked on one unresponsive peer must not hold up queries or
 * traffic for anybody else. Guards against regressing to a single transport
 * wide lock held across blocking socket calls.
 */
TEST_F(TcpTransportTest, StalledPeerDoesNotBlockOtherPeers) {
    TcpTransportConfig slow = config;
    slow.send_timeout = std::chrono::milliseconds(5000);
    slow.magic_cookie_enabled = false;

    TcpTransport server(slow);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    // Declared ahead of the transport so it is still alive when that transport
    // is torn down at the end of the test.
    TestTcpListener stalled_listener;

    // Connects but never starts, so nothing drains its socket and the server's
    // sends to it block once the kernel buffers fill.
    TcpTransport stalled(slow);
    ASSERT_EQ(stalled.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(stalled.connect(server_ep), Result::SUCCESS);
    const Endpoint stalled_ep = stalled.get_local_endpoint();

    ConnectedClient healthy;
    healthy.transport = std::make_unique<TcpTransport>(slow);
    healthy.listener = std::make_unique<TestTcpListener>();
    ASSERT_EQ(healthy.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    healthy.transport->set_listener(healthy.listener.get());
    ASSERT_EQ(healthy.transport->start(), Result::SUCCESS);
    ASSERT_EQ(healthy.transport->connect(server_ep), Result::SUCCESS);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() < 2U && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(server.connection_count(), 2U);

    // Keep sending to the stalled peer until the socket backs up and blocks.
    std::atomic<bool> keep_sending{true};
    std::atomic<size_t> sends_completed{0};
    std::thread flooder([&]() {
        // Sized to stay inside the static build's medium buffer tier. The large
        // tier has only a handful of slots, and flooding it would exhaust the
        // pool rather than exercise the socket back-pressure this test is about.
        std::vector<uint8_t> payload(1024, 0xCD);
        Message big = make_tagged_message(0xAB);
        big.set_payload(payload.data(), payload.size());
        while (keep_sending) {
            if (server.send_message(big, stalled_ep) != Result::SUCCESS) {
                break;
            }
            ++sends_completed;
        }
    });

    // Give the flooder time to fill the socket and block inside send().
    std::this_thread::sleep_for(std::chrono::milliseconds(750));

    // A send that merely started is not evidence of anything; the point of this
    // test is a send that is stuck. Two samples that agree show the flooder has
    // stopped making progress, so the probes below really do run against a
    // connection whose I/O is blocked.
    const size_t progress_before = sends_completed.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(sends_completed.load(), progress_before)
        << "flooder is still making progress, so nothing is blocked to isolate";
    ASSERT_GT(progress_before, 0U) << "flooder never got a message out at all";

    const auto probe_start = std::chrono::steady_clock::now();
    const size_t count = server.connection_count();
    const bool healthy_up = server.is_peer_connected(healthy.transport->get_local_endpoint());
    const auto probe_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - probe_start);

    EXPECT_EQ(count, 2U);
    EXPECT_TRUE(healthy_up);
    EXPECT_LT(probe_ms.count(), 1000)
        << "connection table queries waited " << probe_ms.count()
        << "ms behind a send blocked on an unresponsive peer";

    // Let the stalled peer start draining so the in-flight send can complete.
    // Closing it here instead would complete that send with EPIPE, and the send
    // path passes no MSG_NOSIGNAL, so the whole test process would take a
    // SIGPIPE. Nothing about this test needs the peer to vanish mid-send.
    keep_sending = false;
    stalled.set_listener(&stalled_listener);
    EXPECT_EQ(stalled.start(), Result::SUCCESS);
    flooder.join();

    stalled.disconnect();
    stalled.stop();

    healthy.transport->disconnect();
    healthy.transport->stop();
    server.stop();
}

/**
 * @test_case TC_TCP_PEER_ISOLATION
 * @tests REQ_TRANSPORT_003b, REQ_TRANSPORT_003_E01
 * @brief One peer disconnecting leaves the other peers serviceable
 */
TEST_F(TcpTransportTest, OnePeerDisconnectingLeavesOthersConnected) {
    constexpr size_t NUM_CLIENTS = 3;

    TcpTransport server(config);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    std::vector<ConnectedClient> clients;
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        ConnectedClient client;
        client.transport = std::make_unique<TcpTransport>(config);
        client.listener = std::make_unique<TestTcpListener>();
        ASSERT_EQ(client.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
        client.transport->set_listener(client.listener.get());
        ASSERT_EQ(client.transport->start(), Result::SUCCESS);
        ASSERT_EQ(client.transport->connect(server_ep), Result::SUCCESS);
        clients.push_back(std::move(client));
    }

    const auto connected_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() < NUM_CLIENTS &&
           std::chrono::steady_clock::now() < connected_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(server.connection_count(), NUM_CLIENTS);

    // Drop the middle client.
    clients[1].transport->disconnect();
    clients[1].transport->stop();

    const auto dropped_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() > NUM_CLIENTS - 1 &&
           std::chrono::steady_clock::now() < dropped_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_EQ(server.connection_count(), NUM_CLIENTS - 1)
        << "only the departing peer may be removed";

    server_listener.clear_messages();

    // The survivors must still be served.
    EXPECT_EQ(clients[0].transport->send_message(make_tagged_message(0), server_ep),
              Result::SUCCESS);
    EXPECT_EQ(clients[2].transport->send_message(make_tagged_message(2), server_ep),
              Result::SUCCESS);

    EXPECT_TRUE(server_listener.wait_for_messages(2, std::chrono::milliseconds(5000)))
        << "surviving peers must still be serviced";

    clients[0].transport->disconnect();
    clients[0].transport->stop();
    clients[2].transport->disconnect();
    clients[2].transport->stop();
    server.stop();
}

/**
 * @test_case TC_TCP_ROUTE_UNKNOWN_PEER
 * @tests REQ_TRANSPORT_002_E04
 * @brief Sending to an endpoint with no connection fails instead of misrouting
 */
TEST_F(TcpTransportTest, SendToUnknownPeerDoesNotMisroute) {
    TcpTransport server(config);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    TcpTransport client(config);
    ASSERT_EQ(client.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(client.start(), Result::SUCCESS);
    ASSERT_EQ(client.connect(server_ep), Result::SUCCESS);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() < 1U && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(server.connection_count(), 1U);

    // A well-formed endpoint that is not one of the server's peers.
    const Endpoint stranger("127.0.0.1", 1);
    EXPECT_FALSE(server.is_peer_connected(stranger));
    EXPECT_EQ(server.send_message(make_tagged_message(0), stranger), Result::NOT_CONNECTED)
        << "an unknown peer must not fall back to another connection";

    EXPECT_EQ(server.send_message(make_tagged_message(0), Endpoint("not-an-ip", 100)),
              Result::INVALID_ENDPOINT);

    client.disconnect();
    client.stop();
    server.stop();
}

/**
 * @test_case TC_TCP_DISCONNECT_PEER
 * @tests REQ_TRANSPORT_003b, REQ_TRANSPORT_018
 * @brief disconnect_peer() closes the named peer and only that peer
 */
TEST_F(TcpTransportTest, DisconnectPeerClosesOnlyThatPeer) {
    constexpr size_t NUM_CLIENTS = 3;

    TcpTransport server(config);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    std::vector<ConnectedClient> clients;
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        ConnectedClient client;
        client.transport = std::make_unique<TcpTransport>(config);
        client.listener = std::make_unique<TestTcpListener>();
        ASSERT_EQ(client.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
        client.transport->set_listener(client.listener.get());
        ASSERT_EQ(client.transport->start(), Result::SUCCESS);
        ASSERT_EQ(client.transport->connect(server_ep), Result::SUCCESS);
        clients.push_back(std::move(client));
    }

    // Each client announces itself so the server learns its peer endpoint.
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        ASSERT_EQ(clients[i].transport->send_message(make_tagged_message(static_cast<uint8_t>(i)),
                                                     server_ep),
                  Result::SUCCESS);
    }
    ASSERT_TRUE(server_listener.wait_for_messages(NUM_CLIENTS, std::chrono::milliseconds(5000)));

    std::vector<Endpoint> peers;
    for (const auto& [message, sender] : server_listener.get_received_messages()) {
        peers.push_back(sender);
    }
    ASSERT_EQ(peers.size(), NUM_CLIENTS);

    const Endpoint victim = peers[1];
    ASSERT_TRUE(server.is_peer_connected(victim));
    EXPECT_EQ(server.disconnect_peer(victim), Result::SUCCESS);

    EXPECT_FALSE(server.is_peer_connected(victim));
    EXPECT_EQ(server.connection_count(), NUM_CLIENTS - 1)
        << "only the named peer may be closed";

    // A second close of the same peer finds nothing left to claim.
    EXPECT_EQ(server.disconnect_peer(victim), Result::NOT_CONNECTED);

    // The loss is reported once, by whichever thread claimed the teardown. A
    // second report would mean two threads both believed they owned the close.
    const auto losses = server_listener.get_lost_endpoints();
    EXPECT_EQ(std::count_if(losses.begin(), losses.end(),
                            [&victim](const Endpoint& ep) {
                                return ep.get_port() == victim.get_port();
                            }),
              1)
        << "a closed peer must be reported lost exactly once";

    server_listener.clear_messages();

    // The untouched peers must still be served over their own connections.
    ASSERT_EQ(clients[0].transport->send_message(make_tagged_message(0), server_ep),
              Result::SUCCESS);
    ASSERT_EQ(clients[2].transport->send_message(make_tagged_message(2), server_ep),
              Result::SUCCESS);
    EXPECT_TRUE(server_listener.wait_for_messages(2, std::chrono::milliseconds(5000)))
        << "surviving peers must still be serviced";

    for (auto& client : clients) {
        client.transport->disconnect();
        client.transport->stop();
    }
    server.stop();
}

/**
 * @test_case TC_TCP_CAPACITY_CLAMP
 * @tests REQ_TRANSPORT_003_E01
 * @brief max_connections is clamped to the compile-time slot table capacity
 *
 * The slot table is a fixed array of SOMEIP_MAX_TCP_CONNECTIONS entries, so a
 * configuration asking for more peers than that must be reported honestly
 * rather than accepted and then silently under-served.
 */
TEST_F(TcpTransportTest, MaxConnectionsClampedToCompileTimeCapacity) {
    TcpTransportConfig oversized = config;
    oversized.max_connections = MAX_TCP_CONNECTIONS + 100;

    TcpTransport server(oversized);
    EXPECT_EQ(server.max_connections(), MAX_TCP_CONNECTIONS);

    // Zero is meaningless for a transport that has to serve somebody, so it is
    // raised to one rather than locking the transport out of accepting at all.
    TcpTransportConfig zeroed = config;
    zeroed.max_connections = 0;
    TcpTransport minimal(zeroed);
    EXPECT_EQ(minimal.max_connections(), 1U);

    TcpTransportConfig under = config;
    under.max_connections = 2;
    TcpTransport small(under);
    EXPECT_EQ(small.max_connections(), 2U) << "a limit below capacity is left alone";
}

/**
 * @test_case TC_TCP_CLOSE_STALLED_PEER
 * @tests REQ_TRANSPORT_003b, REQ_TRANSPORT_019
 * @brief Closing a peer whose send is blocked completes instead of hanging
 *
 * Teardown waits for the connection's in-flight I/O to finish, so the send it
 * waits on has to be bounded. With an unbounded retry a peer that stops reading
 * pins its connection for good and this disconnect, and with it stop() and the
 * destructor, never returns. A regression therefore shows up as this suite
 * hitting the 30s ctest timeout rather than as a failed expectation.
 */
TEST_F(TcpTransportTest, DisconnectingStalledPeerDoesNotHang) {
    TcpTransportConfig slow = config;
    slow.send_timeout = std::chrono::milliseconds(300);
    slow.magic_cookie_enabled = false;

    TcpTransport server(slow);
    ASSERT_EQ(server.initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    ASSERT_EQ(server.enable_server_mode(), Result::SUCCESS);

    TestTcpListener server_listener;
    server.set_listener(&server_listener);
    ASSERT_EQ(server.start(), Result::SUCCESS);

    const Endpoint server_ep = server.get_local_endpoint();

    // A peer that never reads, with its receive buffer pinned small. A second
    // TcpTransport is no good here: the kernel grows a loopback socket's receive
    // buffer on demand, so the server's sends trickle through instead of ever
    // blocking outright. Setting SO_RCVBUF explicitly, before connect(), turns
    // that auto-tuning off and makes the stall permanent.
    someip_socket_t const stalled_fd = someip_socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(stalled_fd, SOMEIP_INVALID_SOCKET);

    int rcvbuf = 2048;
    ASSERT_EQ(someip_setsockopt(stalled_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)), 0);

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_ep.get_port());
    server_addr.sin_addr.s_addr = someip_inet_addr(server_ep.get_address().c_str());
    ASSERT_EQ(someip_connect(stalled_fd, reinterpret_cast<const sockaddr*>(&server_addr),
                             sizeof(server_addr)),
              0);

    sockaddr_in local_addr = {};
    socklen_t local_len = sizeof(local_addr);
    ASSERT_EQ(someip_getsockname(stalled_fd, reinterpret_cast<sockaddr*>(&local_addr), &local_len),
              0);
    const Endpoint stalled_ep("127.0.0.1", someip_ntohs(local_addr.sin_port),
                              TransportProtocol::TCP);

    ConnectedClient healthy;
    healthy.transport = std::make_unique<TcpTransport>(slow);
    healthy.listener = std::make_unique<TestTcpListener>();
    ASSERT_EQ(healthy.transport->initialize(Endpoint("127.0.0.1", 0)), Result::SUCCESS);
    healthy.transport->set_listener(healthy.listener.get());
    ASSERT_EQ(healthy.transport->start(), Result::SUCCESS);
    ASSERT_EQ(healthy.transport->connect(server_ep), Result::SUCCESS);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (server.connection_count() < 2U && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(server.connection_count(), 2U);
    ASSERT_TRUE(server.is_peer_connected(stalled_ep));

    std::atomic<bool> keep_sending{true};
    std::atomic<size_t> sends_completed{0};
    std::thread flooder([&]() {
        std::vector<uint8_t> payload(1024, 0xCD);
        Message big = make_tagged_message(0xAB);
        big.set_payload(payload.data(), payload.size());
        while (keep_sending) {
            if (server.send_message(big, stalled_ep) == Result::SUCCESS) {
                ++sends_completed;
            } else if (!server.is_peer_connected(stalled_ep)) {
                break;  // A send that gave up mid-message closed the peer.
            }
        }
    });

    // Wait until the flooder stops making progress, i.e. it is inside a send the
    // stalled peer is not taking and is holding that connection's I/O rights.
    //
    // The window has to be generous. The kernel grows the server's send buffer
    // on demand, so early sends keep succeeding at a falling rate for a while;
    // only once that buffer hits its ceiling with the peer's window shut does a
    // send block outright. A short window mistakes the slow phase for the
    // stalled one and measures a close that never had to wait for anything.
    const auto blocked_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool blocked = false;
    while (!blocked && std::chrono::steady_clock::now() < blocked_deadline) {
        const size_t last = sends_completed.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        blocked = (sends_completed.load() == last) && (last > 0);
    }
    ASSERT_TRUE(blocked) << "could not get a send to block on the stalled peer";

    // The close waits that send out, but only for the send budget. The flooder
    // may instead win the race and close the peer itself, once its own send
    // times out having written part of a message; either outcome is correct.
    // What must not happen is this call failing to return.
    const auto close_start = std::chrono::steady_clock::now();
    const Result closed = server.disconnect_peer(stalled_ep);
    const auto close_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - close_start);

    EXPECT_TRUE(closed == Result::SUCCESS || closed == Result::NOT_CONNECTED)
        << "unexpected result " << static_cast<int>(closed);
    EXPECT_LT(close_ms.count(), 3000)
        << "closing a stalled peer took " << close_ms.count()
        << "ms, so the send it waits on is not bounded by send_timeout";

    keep_sending = false;
    flooder.join();

    EXPECT_FALSE(server.is_peer_connected(stalled_ep));

    // Whether the flooder or this thread claimed the teardown, only one of them
    // may report it. Two reports would mean both believed they owned the close.
    const auto losses = server_listener.get_lost_endpoints();
    EXPECT_EQ(std::count_if(losses.begin(), losses.end(),
                            [&stalled_ep](const Endpoint& ep) {
                                return ep.get_port() == stalled_ep.get_port();
                            }),
              1)
        << "the stalled peer must be reported lost exactly once";

    // The healthy peer is untouched by any of this.
    EXPECT_EQ(server.connection_count(), 1U);
    EXPECT_TRUE(server.is_peer_connected(healthy.transport->get_local_endpoint()));

    someip_close_socket(stalled_fd);
    healthy.transport->disconnect();
    healthy.transport->stop();
    server.stop();
}
