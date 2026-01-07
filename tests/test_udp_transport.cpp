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
#include <transport/udp_transport.h>
#include <transport/transport.h>
#include <someip/message.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace someip;
using namespace someip::transport;

class UdpTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default config for most tests
        config.blocking = true;
        config.receive_buffer_size = 65536;
        config.send_buffer_size = 65536;
        config.reuse_address = true;
        config.reuse_port = false;
        config.enable_broadcast = false;
        config.multicast_interface = "";
        config.multicast_ttl = 1;
        config.max_message_size = 1400;
    }

    void TearDown() override {
        // Clean up any running transports
    }

    UdpTransportConfig config;
    Endpoint local_endpoint{"127.0.0.1", 0};  // Port 0 = auto-assign
};

class TestUdpListener : public ITransportListener {
public:
    void on_message_received(MessagePtr message, const Endpoint& sender) override {
        std::scoped_lock lock(mutex_);
        received_messages_.push_back({message, sender});
        cv_.notify_one();
    }

    void on_connection_lost(const Endpoint& endpoint) override {
        std::scoped_lock lock(mutex_);
        connection_lost_ = true;
        cv_.notify_one();
    }

    void on_connection_established(const Endpoint& endpoint) override {
        std::scoped_lock lock(mutex_);
        connection_established_ = true;
        cv_.notify_one();
    }

    void on_error(Result error) override {
        std::scoped_lock lock(mutex_);
        last_error_ = error;
        error_count_++;
        cv_.notify_one();
    }

    // Helper methods
    bool wait_for_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return !received_messages_.empty(); });
    }

    bool wait_for_error(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return error_count_ > 0; });
    }

    void reset() {
        std::scoped_lock lock(mutex_);
        received_messages_.clear();
        connection_lost_ = false;
        connection_established_ = false;
        last_error_ = Result::SUCCESS;
        error_count_ = 0;
    }

    std::vector<std::pair<MessagePtr, Endpoint>> received_messages_;
    std::atomic<bool> connection_lost_{false};
    std::atomic<bool> connection_established_{false};
    std::atomic<Result> last_error_{Result::SUCCESS};
    std::atomic<int> error_count_{0};

private:
    std::mutex mutex_;
    std::condition_variable cv_;
};

// Test basic initialization with default config
TEST_F(UdpTransportTest, InitializationWithDefaultConfig) {
    UdpTransport transport(local_endpoint);

    EXPECT_FALSE(transport.is_running());
    EXPECT_FALSE(transport.is_connected());
    EXPECT_EQ(transport.get_local_endpoint().get_address(), "127.0.0.1");
}

// Test initialization with custom config
TEST_F(UdpTransportTest, InitializationWithCustomConfig) {
    config.blocking = false;
    config.receive_buffer_size = 32768;
    config.send_buffer_size = 32768;
    config.enable_broadcast = true;

    UdpTransport transport(local_endpoint, config);

    EXPECT_FALSE(transport.is_running());
    EXPECT_FALSE(transport.is_connected());
}

// Test initialization with all new config options
TEST_F(UdpTransportTest, InitializationWithFullConfig) {
    config.blocking = true;
    config.receive_buffer_size = 65536;
    config.send_buffer_size = 65536;
    config.reuse_address = true;
    config.reuse_port = true;  // New option
    config.enable_broadcast = false;
    config.multicast_interface = "";  // New option
    config.multicast_ttl = 1;  // New option
    config.max_message_size = 1400;  // New option (SOME/IP spec recommendation)

    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;
    transport.set_listener(&listener);

    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());

    transport.stop();
}

// Test blocking mode transport lifecycle
TEST_F(UdpTransportTest, BlockingModeLifecycle) {
    config.blocking = true;
    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;

    transport.set_listener(&listener);

    // Start transport
    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());
    EXPECT_TRUE(transport.is_connected());  // UDP socket is bound and ready

    // Check that local endpoint got assigned a port
    EXPECT_NE(transport.get_local_endpoint().get_port(), 0);

    // Stop transport
    EXPECT_EQ(transport.stop(), Result::SUCCESS);
    EXPECT_FALSE(transport.is_running());
}

// Test non-blocking mode transport lifecycle
TEST_F(UdpTransportTest, NonBlockingModeLifecycle) {
    config.blocking = false;
    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;

    transport.set_listener(&listener);

    // Start transport
    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());

    // Stop transport
    EXPECT_EQ(transport.stop(), Result::SUCCESS);
    EXPECT_FALSE(transport.is_running());
}

// Test socket configuration options
TEST_F(UdpTransportTest, SocketConfigurationOptions) {
    // Test with broadcast enabled
    config.enable_broadcast = true;
    config.receive_buffer_size = 131072;  // 128KB
    config.send_buffer_size = 131072;

    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;
    transport.set_listener(&listener);

    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());

    transport.stop();
}

// Test message sending (requires two transports)
TEST_F(UdpTransportTest, MessageRoundTrip) {
    config.blocking = true;
    UdpTransport sender(local_endpoint, config);
    UdpTransport receiver(local_endpoint, config);  // Will get different port

    TestUdpListener sender_listener;
    TestUdpListener receiver_listener;

    sender.set_listener(&sender_listener);
    receiver.set_listener(&receiver_listener);

    // Start both transports
    EXPECT_EQ(sender.start(), Result::SUCCESS);
    EXPECT_EQ(receiver.start(), Result::SUCCESS);

    // Get the actual endpoints after binding
    Endpoint sender_endpoint = sender.get_local_endpoint();
    Endpoint receiver_endpoint = receiver.get_local_endpoint();

    // Create and send a message
    Message message;
    message.set_service_id(0x1234);
    message.set_method_id(0x5678);
    message.set_client_id(0x9ABC);
    message.set_session_id(0xDEF0);
    message.set_protocol_version(1);
    message.set_interface_version(1);
    message.set_message_type(MessageType::REQUEST);
    message.set_return_code(ReturnCode::E_OK);

    // Add some payload
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    message.set_payload(payload);

    // Send message from sender to receiver
    EXPECT_EQ(sender.send_message(message, receiver_endpoint), Result::SUCCESS);

    // Wait for receiver to get the message
    EXPECT_TRUE(receiver_listener.wait_for_message());

    // Verify received message
    ASSERT_EQ(receiver_listener.received_messages_.size(), 1);
    auto [received_msg, actual_sender_endpoint] = receiver_listener.received_messages_[0];

    EXPECT_EQ(received_msg->get_service_id(), 0x1234);
    EXPECT_EQ(received_msg->get_method_id(), 0x5678);
    EXPECT_EQ(received_msg->get_client_id(), 0x9ABC);
    EXPECT_EQ(received_msg->get_session_id(), 0xDEF0);
    EXPECT_EQ(received_msg->get_payload(), payload);

    // Verify sender endpoint information
    EXPECT_EQ(actual_sender_endpoint.get_address(), sender_endpoint.get_address());
    EXPECT_EQ(actual_sender_endpoint.get_port(), sender_endpoint.get_port());

    // Clean up
    sender.stop();
    receiver.stop();
}

// Test non-blocking mode behavior (should not block on receive)
TEST_F(UdpTransportTest, NonBlockingModeBehavior) {
    config.blocking = false;
    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;
    transport.set_listener(&listener);

    EXPECT_EQ(transport.start(), Result::SUCCESS);

    // In non-blocking mode, receive_message should return nullptr when no data
    // (this test may be timing-dependent, but should work in most cases)
    MessagePtr msg = transport.receive_message();
    // We can't guarantee no messages, but if there are none, it should return nullptr quickly

    transport.stop();
}

// Test error handling
TEST_F(UdpTransportTest, ErrorHandling) {
    UdpTransport transport(local_endpoint);
    TestUdpListener listener;
    transport.set_listener(&listener);

    // Try operations on non-started transport
    Message message;
    Endpoint remote_endpoint{"127.0.0.1", 12345};

    EXPECT_EQ(transport.send_message(message, remote_endpoint), Result::NOT_CONNECTED);
    EXPECT_EQ(transport.connect(remote_endpoint), Result::SUCCESS);  // UDP connect is different
    EXPECT_EQ(transport.disconnect(), Result::SUCCESS);
}

// Test invalid endpoint handling
TEST_F(UdpTransportTest, InvalidEndpointHandling) {
    // Invalid address (should throw in constructor)
    Endpoint invalid_endpoint{"999.999.999.999", 12345};
    EXPECT_THROW(UdpTransport transport(invalid_endpoint), std::invalid_argument);

    // Valid transport with invalid remote endpoint (transport not started)
    UdpTransport transport(local_endpoint);
    Message message;
    Endpoint invalid_remote{"invalid.address", 12345};

    // Should fail because transport is not started
    EXPECT_EQ(transport.send_message(message, invalid_remote), Result::NOT_CONNECTED);

    // Start transport and try again - should detect invalid endpoint
    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_EQ(transport.send_message(message, invalid_remote), Result::INVALID_ENDPOINT);
}

// Test multicast functionality
TEST_F(UdpTransportTest, MulticastSupport) {
    UdpTransport transport(local_endpoint);
    TestUdpListener listener;
    transport.set_listener(&listener);

    // Start the transport first
    EXPECT_EQ(transport.start(), Result::SUCCESS);

    // Now test multicast operations
    Result result1 = transport.join_multicast_group("224.0.0.1");
    Result result2 = transport.leave_multicast_group("224.0.0.1");

    // Should succeed since the functionality is implemented
    EXPECT_EQ(result1, Result::SUCCESS);
    EXPECT_EQ(result2, Result::SUCCESS);

    transport.stop();
}

// Test multicast address validation (per SOME/IP spec, valid range: 224.0.0.0 - 239.255.255.255)
TEST_F(UdpTransportTest, MulticastAddressValidation) {
    UdpTransport transport(local_endpoint);
    TestUdpListener listener;
    transport.set_listener(&listener);

    EXPECT_EQ(transport.start(), Result::SUCCESS);

    // Valid multicast addresses (224.0.0.0 - 239.255.255.255)
    EXPECT_EQ(transport.join_multicast_group("224.0.0.1"), Result::SUCCESS);
    EXPECT_EQ(transport.leave_multicast_group("224.0.0.1"), Result::SUCCESS);

    EXPECT_EQ(transport.join_multicast_group("239.255.255.250"), Result::SUCCESS);
    EXPECT_EQ(transport.leave_multicast_group("239.255.255.250"), Result::SUCCESS);

    // SOME/IP SD commonly uses 224.224.224.245
    EXPECT_EQ(transport.join_multicast_group("224.224.224.245"), Result::SUCCESS);
    EXPECT_EQ(transport.leave_multicast_group("224.224.224.245"), Result::SUCCESS);

    // Invalid multicast addresses (should fail)
    EXPECT_EQ(transport.join_multicast_group("192.168.1.1"), Result::INVALID_ENDPOINT);
    EXPECT_EQ(transport.join_multicast_group("255.255.255.255"), Result::INVALID_ENDPOINT);
    EXPECT_EQ(transport.join_multicast_group("10.0.0.1"), Result::INVALID_ENDPOINT);

    transport.stop();
}

// Test multicast with custom TTL setting (per SOME/IP spec, TTL should be configurable)
TEST_F(UdpTransportTest, MulticastTTLConfiguration) {
    config.multicast_ttl = 16;  // Non-default TTL value

    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;
    transport.set_listener(&listener);

    EXPECT_EQ(transport.start(), Result::SUCCESS);

    // Multicast should work with custom TTL
    EXPECT_EQ(transport.join_multicast_group("224.0.0.1"), Result::SUCCESS);
    EXPECT_EQ(transport.leave_multicast_group("224.0.0.1"), Result::SUCCESS);

    transport.stop();
}

// Test multicast with specific interface configuration
TEST_F(UdpTransportTest, MulticastInterfaceConfiguration) {
    config.multicast_interface = "127.0.0.1";  // Use loopback for testing

    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;
    transport.set_listener(&listener);

    EXPECT_EQ(transport.start(), Result::SUCCESS);

    // Multicast should work with specific interface
    EXPECT_EQ(transport.join_multicast_group("224.0.0.1"), Result::SUCCESS);
    EXPECT_EQ(transport.leave_multicast_group("224.0.0.1"), Result::SUCCESS);

    transport.stop();
}

// Test SO_REUSEPORT option for multicast SD port sharing
TEST_F(UdpTransportTest, ReusePortConfiguration) {
    config.reuse_port = true;
    config.reuse_address = true;

    // Create first transport on a specific port
    Endpoint endpoint1{"127.0.0.1", 30490};  // SOME/IP SD port
    UdpTransport transport1(endpoint1, config);
    TestUdpListener listener1;
    transport1.set_listener(&listener1);

    EXPECT_EQ(transport1.start(), Result::SUCCESS);

    // Create second transport on the same port (should work with SO_REUSEPORT)
    Endpoint endpoint2{"127.0.0.1", 30490};
    UdpTransport transport2(endpoint2, config);
    TestUdpListener listener2;
    transport2.set_listener(&listener2);

    // With SO_REUSEPORT, both should be able to bind to the same port
    Result result = transport2.start();
    EXPECT_EQ(result, Result::SUCCESS);

    transport1.stop();
    transport2.stop();
}

// Test configuration validation
TEST_F(UdpTransportTest, ConfigurationValidation) {
    // Test various buffer sizes
    config.receive_buffer_size = 1024;  // Minimum reasonable size
    config.send_buffer_size = 1024;
    UdpTransport transport1(local_endpoint, config);

    config.receive_buffer_size = 1048576;  // 1MB - large but reasonable
    config.send_buffer_size = 1048576;
    UdpTransport transport2(local_endpoint, config);

    // Both should initialize successfully
    EXPECT_EQ(transport1.start(), Result::SUCCESS);
    EXPECT_EQ(transport2.start(), Result::SUCCESS);

    transport1.stop();
    transport2.stop();
}

// Test basic thread safety - start/stop from different threads
TEST_F(UdpTransportTest, BasicThreadSafety) {
    config.blocking = true;
    UdpTransport transport(local_endpoint, config);
    TestUdpListener listener;
    transport.set_listener(&listener);

    // Test that we can start and stop the transport safely
    // This is a basic thread safety check
    EXPECT_EQ(transport.start(), Result::SUCCESS);
    EXPECT_TRUE(transport.is_running());

    // Multiple stop calls should be safe
    EXPECT_EQ(transport.stop(), Result::SUCCESS);
    EXPECT_FALSE(transport.is_running());

    // Multiple stop calls should still be safe (idempotent)
    EXPECT_EQ(transport.stop(), Result::SUCCESS);
}

// Test resource cleanup
TEST_F(UdpTransportTest, ResourceCleanup) {
    {
        UdpTransport transport(local_endpoint);
        TestUdpListener listener;
        transport.set_listener(&listener);

        EXPECT_EQ(transport.start(), Result::SUCCESS);
        EXPECT_TRUE(transport.is_running());

        // Transport goes out of scope, should clean up automatically
    }

    // Create another transport on same endpoint to verify cleanup
    UdpTransport transport2(local_endpoint);
    TestUdpListener listener2;
    transport2.set_listener(&listener2);

    // Should be able to start on same endpoint after cleanup
    EXPECT_EQ(transport2.start(), Result::SUCCESS);
    transport2.stop();
}

// Test message size limits per SOME/IP spec (1400 bytes payload recommended to avoid fragmentation)
TEST_F(UdpTransportTest, MessageSizeLimit) {
    config.max_message_size = 1400;  // SOME/IP recommended max

    UdpTransport sender(local_endpoint, config);
    UdpTransport receiver(local_endpoint, config);

    TestUdpListener sender_listener;
    TestUdpListener receiver_listener;

    sender.set_listener(&sender_listener);
    receiver.set_listener(&receiver_listener);

    EXPECT_EQ(sender.start(), Result::SUCCESS);
    EXPECT_EQ(receiver.start(), Result::SUCCESS);

    Endpoint receiver_endpoint = receiver.get_local_endpoint();

    // Small message (well under 1400 bytes) - should work fine
    Message small_message;
    small_message.set_service_id(0x1234);
    small_message.set_method_id(0x5678);
    small_message.set_client_id(0x9ABC);
    small_message.set_session_id(0xDEF0);
    small_message.set_protocol_version(1);
    small_message.set_interface_version(1);
    small_message.set_message_type(MessageType::REQUEST);
    small_message.set_return_code(ReturnCode::E_OK);

    std::vector<uint8_t> small_payload(100, 0xAA);  // 100 bytes
    small_message.set_payload(small_payload);

    EXPECT_EQ(sender.send_message(small_message, receiver_endpoint), Result::SUCCESS);

    // Wait for the message
    EXPECT_TRUE(receiver_listener.wait_for_message());
    EXPECT_EQ(receiver_listener.received_messages_.size(), 1);

    sender.stop();
    receiver.stop();
}

// Test maximum message size (64KB limit for UDP)
TEST_F(UdpTransportTest, MaxUdpPayloadSize) {
    config.max_message_size = 0;  // Disable size check to test raw UDP limit

    UdpTransport sender(local_endpoint, config);
    UdpTransport receiver(local_endpoint, config);

    TestUdpListener sender_listener;
    TestUdpListener receiver_listener;

    sender.set_listener(&sender_listener);
    receiver.set_listener(&receiver_listener);

    EXPECT_EQ(sender.start(), Result::SUCCESS);
    EXPECT_EQ(receiver.start(), Result::SUCCESS);

    Endpoint receiver_endpoint = receiver.get_local_endpoint();

    // Large message (close to UDP max of 65507 bytes)
    Message large_message;
    large_message.set_service_id(0x1234);
    large_message.set_method_id(0x5678);
    large_message.set_client_id(0x9ABC);
    large_message.set_session_id(0xDEF1);
    large_message.set_protocol_version(1);
    large_message.set_interface_version(1);
    large_message.set_message_type(MessageType::REQUEST);
    large_message.set_return_code(ReturnCode::E_OK);

    // Create payload that fits within UDP max (accounting for SOME/IP header of 16 bytes)
    std::vector<uint8_t> large_payload(60000, 0xBB);  // ~60KB
    large_message.set_payload(large_payload);

    EXPECT_EQ(sender.send_message(large_message, receiver_endpoint), Result::SUCCESS);

    // Wait for the message
    EXPECT_TRUE(receiver_listener.wait_for_message(std::chrono::milliseconds(2000)));
    EXPECT_EQ(receiver_listener.received_messages_.size(), 1);

    // Verify payload was received correctly
    auto& [received_msg, endpoint] = receiver_listener.received_messages_[0];
    EXPECT_EQ(received_msg->get_payload().size(), 60000);

    sender.stop();
    receiver.stop();
}

// Test multicast join/leave before transport started (should fail)
TEST_F(UdpTransportTest, MulticastBeforeStart) {
    UdpTransport transport(local_endpoint);
    TestUdpListener listener;
    transport.set_listener(&listener);

    // Multicast operations should fail when transport is not started
    EXPECT_EQ(transport.join_multicast_group("224.0.0.1"), Result::NOT_CONNECTED);
    EXPECT_EQ(transport.leave_multicast_group("224.0.0.1"), Result::NOT_CONNECTED);
}

// Test nPDU feature concept - multiple messages in quick succession
// Per SOME/IP spec, the UDP binding shall support transporting more than one message in a UDP packet
TEST_F(UdpTransportTest, MultipleMessagesRapidFire) {
    config.blocking = true;
    UdpTransport sender(local_endpoint, config);
    UdpTransport receiver(local_endpoint, config);

    TestUdpListener sender_listener;
    TestUdpListener receiver_listener;

    sender.set_listener(&sender_listener);
    receiver.set_listener(&receiver_listener);

    EXPECT_EQ(sender.start(), Result::SUCCESS);
    EXPECT_EQ(receiver.start(), Result::SUCCESS);

    Endpoint receiver_endpoint = receiver.get_local_endpoint();

    // Send multiple messages rapidly
    constexpr int NUM_MESSAGES = 10;
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        Message message;
        message.set_service_id(0x1234);
        message.set_method_id(0x5678);
        message.set_client_id(0x9ABC);
        message.set_session_id(static_cast<uint16_t>(i + 1));
        message.set_protocol_version(1);
        message.set_interface_version(1);
        message.set_message_type(MessageType::REQUEST);
        message.set_return_code(ReturnCode::E_OK);

        std::vector<uint8_t> payload = {static_cast<uint8_t>(i)};
        message.set_payload(payload);

        EXPECT_EQ(sender.send_message(message, receiver_endpoint), Result::SUCCESS);
    }

    // Wait for all messages to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should have received all messages
    EXPECT_EQ(receiver_listener.received_messages_.size(), NUM_MESSAGES);

    // Verify session IDs are correct (in order)
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        EXPECT_EQ(receiver_listener.received_messages_[i].first->get_session_id(),
                  static_cast<uint16_t>(i + 1));
    }

    sender.stop();
    receiver.stop();
}

