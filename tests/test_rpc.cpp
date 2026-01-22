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
#include <thread>
#include <chrono>
#include <atomic>

using namespace someip::rpc;

/**
 * @brief RPC (Request/Response) unit tests
 * @tests REQ_ARCH_001
 * @tests REQ_ARCH_002
 * @tests feat_req_someip_700
 * @tests feat_req_someip_710
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
    auto handler = [](uint16_t client_id, uint16_t session_id,
                     const std::vector<uint8_t>& input,
                     std::vector<uint8_t>& output) -> RpcResult {
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
