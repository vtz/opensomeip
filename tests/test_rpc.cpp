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
#include <rpc/rpc_types.h>
#include <rpc/rpc_client.h>
#include <rpc/rpc_server.h>
#include <someip/message.h>
#include <someip/types.h>
#include <transport/endpoint.h>
#include <transport/udp_transport.h>
#include <common/result.h>
#include <thread>
#include <chrono>
#include <atomic>

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "static_pool_init.h"

using namespace someip;
using namespace someip::rpc;

/**
 * @brief RPC (Request/Response) unit tests
 * @tests REQ_ARCH_001
 * @tests REQ_ARCH_002
 * @tests feat_req_someip_700
 * @tests feat_req_someip_710
 * @tests REQ_MSG_114, REQ_MSG_115, REQ_MSG_116, REQ_MSG_118
 * @tests REQ_MSG_127, REQ_MSG_128, REQ_MSG_129, REQ_MSG_130, REQ_MSG_131
 * @tests REQ_MSG_132a, REQ_MSG_132b, REQ_MSG_133a, REQ_MSG_133b, REQ_MSG_133c
 */
class RpcTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test service and method IDs
        test_service_id_ = 0x1234;
        test_method_id_ = 0x0001;
        client_id_ = 0xABCD;
    }

    void TearDown() override {
        // Cleanup if needed
    }

    uint16_t test_service_id_;
    uint16_t test_method_id_;
    uint16_t client_id_;
};

// Test RPC types
TEST_F(RpcTest, RpcResultValues) {
    EXPECT_EQ(static_cast<int>(RpcResult::SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(RpcResult::TIMEOUT), 1);
    EXPECT_EQ(static_cast<int>(RpcResult::NETWORK_ERROR), 2);
    EXPECT_EQ(static_cast<int>(RpcResult::INVALID_PARAMETERS), 3);
    EXPECT_EQ(static_cast<int>(RpcResult::METHOD_NOT_FOUND), 4);
    EXPECT_EQ(static_cast<int>(RpcResult::SERVICE_NOT_AVAILABLE), 5);
    EXPECT_EQ(static_cast<int>(RpcResult::INTERNAL_ERROR), 6);
}

TEST_F(RpcTest, RpcRequestConstruction) {
    RpcRequest request(test_service_id_, test_method_id_, client_id_, 0x1234);

    EXPECT_EQ(request.service_id, test_service_id_);
    EXPECT_EQ(request.method_id, test_method_id_);
    EXPECT_EQ(request.client_id, client_id_);
    EXPECT_EQ(request.session_id, 0x1234u);
    EXPECT_TRUE(request.parameters.empty());
}

TEST_F(RpcTest, RpcResponseConstruction) {
    RpcResponse response(test_service_id_, test_method_id_, client_id_, 0x1234, RpcResult::SUCCESS);

    EXPECT_EQ(response.service_id, test_service_id_);
    EXPECT_EQ(response.method_id, test_method_id_);
    EXPECT_EQ(response.client_id, client_id_);
    EXPECT_EQ(response.session_id, 0x1234u);
    EXPECT_EQ(response.result, RpcResult::SUCCESS);
    EXPECT_TRUE(response.return_values.empty());
}

// Test server method registration
TEST_F(RpcTest, ServerMethodRegistration) {
    RpcServer server(test_service_id_);

    // Should be able to register a method
    auto handler = [](uint16_t /*client_id*/, uint16_t /*session_id*/,
                     const platform::ByteBuffer& /*input*/,
                     platform::ByteBuffer& output) -> RpcResult {
        output = {0x01, 0x02, 0x03};
        return RpcResult::SUCCESS;
    };

    EXPECT_TRUE(server.register_method(test_method_id_, handler));
    EXPECT_TRUE(server.is_method_registered(test_method_id_));

    auto methods = server.get_registered_methods();
    EXPECT_EQ(methods.size(), 1u);
    EXPECT_EQ(methods[0], test_method_id_);

    // Should not be able to register the same method twice
    EXPECT_FALSE(server.register_method(test_method_id_, handler));

    // Should be able to unregister
    EXPECT_TRUE(server.unregister_method(test_method_id_));
    EXPECT_FALSE(server.is_method_registered(test_method_id_));

    // Unregistering non-existent method should fail
    EXPECT_FALSE(server.unregister_method(test_method_id_));
}

// Test client basic functionality
TEST_F(RpcTest, ClientBasicFunctionality) {
    RpcClient client(client_id_);

    EXPECT_FALSE(client.is_ready());

    // Initialize client
    EXPECT_TRUE(client.initialize());
    EXPECT_TRUE(client.is_ready());

    // Shutdown client
    client.shutdown();
    EXPECT_FALSE(client.is_ready());
}

// Test timeout configuration
TEST_F(RpcTest, RpcTimeoutConfiguration) {
    RpcTimeout timeout;

    // Default values
    EXPECT_EQ(timeout.request_timeout, std::chrono::milliseconds(1000));
    EXPECT_EQ(timeout.response_timeout, std::chrono::milliseconds(5000));

    // Custom values
    RpcTimeout custom_timeout;
    custom_timeout.request_timeout = std::chrono::milliseconds(500);
    custom_timeout.response_timeout = std::chrono::milliseconds(2000);

    EXPECT_EQ(custom_timeout.request_timeout, std::chrono::milliseconds(500));
    EXPECT_EQ(custom_timeout.response_timeout, std::chrono::milliseconds(2000));
}

// Test statistics structure
TEST_F(RpcTest, ClientStatistics) {
    RpcClient client(client_id_);

    auto stats = client.get_statistics();

    // Initially all zeros (TODO: implement actual statistics tracking)
    EXPECT_EQ(stats.total_calls, 0u);
    EXPECT_EQ(stats.successful_calls, 0u);
    EXPECT_EQ(stats.failed_calls, 0u);
    EXPECT_EQ(stats.timeout_calls, 0u);
    EXPECT_EQ(stats.average_response_time, std::chrono::milliseconds(0));
}

TEST_F(RpcTest, ServerStatistics) {
    RpcServer server(test_service_id_);

    auto stats = server.get_statistics();

    // Initially all zeros (TODO: implement actual statistics tracking)
    EXPECT_EQ(stats.total_calls_received, 0u);
    EXPECT_EQ(stats.successful_calls, 0u);
    EXPECT_EQ(stats.failed_calls, 0u);
    EXPECT_EQ(stats.method_not_found_errors, 0u);
    EXPECT_EQ(stats.average_processing_time, std::chrono::milliseconds(0));
}

namespace {

MessagePtr wait_for_udp_message(transport::UdpTransport& transport, int retries = 80) {
    for (int i = 0; i < retries; ++i) {
        MessagePtr msg = transport.receive_message();
        if (msg) {
            return msg;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return nullptr;
}

}  // namespace

/**
 * @tests REQ_MSG_041
 * @brief RpcClient does not send to the SD port when no endpoint is configured
 */
TEST_F(RpcTest, ClientFailsWithoutRemoteEndpoint) {
    RpcClient client(client_id_);
    ASSERT_TRUE(client.initialize());

    RpcTimeout timeout;
    timeout.response_timeout = std::chrono::milliseconds(200);
    auto result = client.call_method_sync(test_service_id_, test_method_id_, {}, timeout);
    EXPECT_EQ(result.result, RpcResult::SERVICE_NOT_AVAILABLE);

    client.shutdown();
}

/**
 * @tests feat_req_someip_92, REQ_MSG_041
 * @brief Default RpcServer bind is not the SD port 30490
 */
TEST_F(RpcTest, ServerDefaultPortIsNotSdPort) {
    RpcServer server(test_service_id_);
    ASSERT_TRUE(server.initialize());
    EXPECT_EQ(server.get_local_endpoint().get_port(), SOMEIP_DEFAULT_RPC_PORT);
    EXPECT_NE(server.get_local_endpoint().get_port(), 30490);
    server.shutdown();
}

/**
 * @tests REQ_MSG_041, feat_req_someip_92
 * @brief Interface Version 0x02 (service major) round-trips request/response
 */
TEST_F(RpcTest, InterfaceVersionTwoRoundTrip) {
    const uint8_t major = 0x02;
    RpcServer server(test_service_id_, major, transport::Endpoint("127.0.0.1", 0));
    ASSERT_TRUE(server.register_method(test_method_id_,
        [](uint16_t, uint16_t, const platform::ByteBuffer& in, platform::ByteBuffer& out) {
            out = in;
            return RpcResult::SUCCESS;
        }));
    ASSERT_TRUE(server.initialize());
    EXPECT_NE(server.get_local_endpoint().get_port(), 30490);
    EXPECT_NE(server.get_local_endpoint().get_port(), 0);

    RpcClient client(client_id_, major);
    ASSERT_TRUE(client.initialize());
    client.set_remote_endpoint(server.get_local_endpoint());

    platform::ByteBuffer params = {0x11, 0x22};
    RpcTimeout timeout;
    timeout.response_timeout = std::chrono::milliseconds(2000);
    auto result = client.call_method_sync(test_service_id_, test_method_id_, params, timeout);
    EXPECT_EQ(result.result, RpcResult::SUCCESS);
    EXPECT_EQ(result.return_values, params);

    client.shutdown();
    server.shutdown();
}

/**
 * @tests REQ_MSG_042, feat_req_someip_92
 * @brief RpcServer returns E_WRONG_INTERFACE_VERSION when request major mismatches
 */
TEST_F(RpcTest, WrongInterfaceVersionReturnsError) {
    RpcServer server(test_service_id_, 0x02, transport::Endpoint("127.0.0.1", 0));
    ASSERT_TRUE(server.register_method(test_method_id_,
        [](uint16_t, uint16_t, const platform::ByteBuffer&, platform::ByteBuffer&) {
            return RpcResult::SUCCESS;
        }));
    ASSERT_TRUE(server.initialize());

    transport::UdpTransport probe(transport::Endpoint("127.0.0.1", 0));
    ASSERT_EQ(probe.start(), Result::SUCCESS);

    Message request(MessageId(test_service_id_, test_method_id_),
                    RequestId(client_id_, 0x0001),
                    MessageType::REQUEST, ReturnCode::E_OK);
    request.set_interface_version(0x01);
    ASSERT_EQ(probe.send_message(request, server.get_local_endpoint()), Result::SUCCESS);

    MessagePtr reply = wait_for_udp_message(probe);
    ASSERT_NE(reply, nullptr);
    EXPECT_EQ(reply->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(reply->get_return_code(), ReturnCode::E_WRONG_INTERFACE_VERSION);
    EXPECT_EQ(reply->get_interface_version(), 0x02);

    probe.stop();
    server.shutdown();
}

/**
 * @tests REQ_MSG_052
 * @brief send_request_no_return writes message type 0x01 and does not wait
 */
TEST_F(RpcTest, FireAndForgetWireTypeNoWait) {
    transport::UdpTransport spy(transport::Endpoint("127.0.0.1", 0));
    ASSERT_EQ(spy.start(), Result::SUCCESS);

    RpcClient client(client_id_, 0x02);
    ASSERT_TRUE(client.initialize());

    platform::ByteBuffer params = {0xAB};
    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(client.send_request_no_return(test_service_id_, test_method_id_,
                                              params, spy.get_local_endpoint()));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, std::chrono::milliseconds(500));

    MessagePtr got = wait_for_udp_message(spy);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->get_message_type(), MessageType::REQUEST_NO_RETURN);
    EXPECT_EQ(static_cast<uint8_t>(got->get_message_type()), 0x01);
    EXPECT_EQ(got->get_interface_version(), 0x02);
    EXPECT_EQ(got->get_return_code(), ReturnCode::E_OK);
    EXPECT_EQ(got->get_payload(), params);

    auto extra = spy.receive_message();
    EXPECT_EQ(extra, nullptr);

    client.shutdown();
    spy.stop();
}

/**
 * @tests REQ_MSG_052
 * @brief Fire-and-forget method does not send RESPONSE; REQUEST gets E_WRONG_MESSAGE_TYPE
 */
TEST_F(RpcTest, FireAndForgetServerDoesNotRespond) {
    std::atomic<int> calls{0};
    RpcServer server(test_service_id_, 0x01, transport::Endpoint("127.0.0.1", 0));
    ASSERT_TRUE(server.register_method(test_method_id_,
        [&calls](uint16_t, uint16_t, const platform::ByteBuffer&, platform::ByteBuffer&) {
            calls.fetch_add(1);
            return RpcResult::SUCCESS;
        }, MethodSemantics::FireAndForget));
    ASSERT_TRUE(server.initialize());

    transport::UdpTransport probe(transport::Endpoint("127.0.0.1", 0));
    ASSERT_EQ(probe.start(), Result::SUCCESS);

    Message no_return(MessageId(test_service_id_, test_method_id_),
                      RequestId(client_id_, 0x0002),
                      MessageType::REQUEST_NO_RETURN, ReturnCode::E_OK);
    ASSERT_EQ(probe.send_message(no_return, server.get_local_endpoint()), Result::SUCCESS);

    for (int i = 0; i < 50 && calls.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(calls.load(), 1);

    MessagePtr unexpected = wait_for_udp_message(probe, 20);
    EXPECT_EQ(unexpected, nullptr);

    Message request(MessageId(test_service_id_, test_method_id_),
                    RequestId(client_id_, 0x0003),
                    MessageType::REQUEST, ReturnCode::E_OK);
    ASSERT_EQ(probe.send_message(request, server.get_local_endpoint()), Result::SUCCESS);

    MessagePtr err = wait_for_udp_message(probe);
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(err->get_return_code(), ReturnCode::E_WRONG_MESSAGE_TYPE);

    probe.stop();
    server.shutdown();
}
