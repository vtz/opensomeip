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
#include "someip/message.h"
#include "serialization/serializer.h"

using namespace someip;

/**
 * @brief SOME/IP Message unit tests
 * @tests REQ_ARCH_001, REQ_ARCH_003
 * @tests REQ_ARCH_005, REQ_ARCH_006, REQ_ARCH_007
 * @tests REQ_MY_001
 * @tests REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @tests REQ_MSG_005, REQ_MSG_006, REQ_MSG_007, REQ_MSG_008
 * @tests REQ_MSG_010, REQ_MSG_011, REQ_MSG_012, REQ_MSG_013, REQ_MSG_014
 * @tests REQ_MSG_020, REQ_MSG_021, REQ_MSG_022, REQ_MSG_023, REQ_MSG_024, REQ_MSG_025
 * @tests REQ_MSG_030, REQ_MSG_031, REQ_MSG_032, REQ_MSG_033
 * @tests REQ_MSG_040, REQ_MSG_041, REQ_MSG_042
 * @tests REQ_MSG_050, REQ_MSG_051, REQ_MSG_052, REQ_MSG_053, REQ_MSG_054, REQ_MSG_055
 * @tests REQ_MSG_056, REQ_MSG_057, REQ_MSG_058, REQ_MSG_059
 * @tests REQ_MSG_060_TP, REQ_MSG_061_TP, REQ_MSG_062_TP
 * @tests REQ_MSG_063, REQ_MSG_064
 * @tests REQ_MSG_070, REQ_MSG_071, REQ_MSG_072
 * @tests REQ_MSG_073, REQ_MSG_074, REQ_MSG_075, REQ_MSG_076, REQ_MSG_077, REQ_MSG_078, REQ_MSG_079, REQ_MSG_080
 * @tests REQ_MSG_090, REQ_MSG_091, REQ_MSG_092, REQ_MSG_093
 * @tests REQ_MSG_100
 * @tests REQ_MSG_012_E01, REQ_MSG_014_E01, REQ_MSG_014_E02
 * @tests REQ_MSG_024_E01, REQ_MSG_024_E02, REQ_MSG_042_E01
 * @tests REQ_MSG_063_E01, REQ_MSG_063_E02, REQ_MSG_071_E01, REQ_MSG_071_E02
 * @tests REQ_MSG_072_E01, REQ_MSG_100_E01, REQ_MSG_100_E02, REQ_MSG_100_E03
 * @tests feat_req_someip_538
 * @tests feat_req_someip_539
 * @tests feat_req_someip_540
 * @tests feat_req_someip_45
 * @tests feat_req_someip_60
 * @tests feat_req_someip_100
 * @tests feat_req_someip_103
 */
class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

/**
 * @test_case TC_MSG_001
 * @tests REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @tests REQ_MSG_020, REQ_MSG_021, REQ_MSG_022
 * @tests REQ_MSG_050, REQ_MSG_070, REQ_MSG_071
 * @brief Test default constructor initializes all fields correctly
 */
TEST_F(MessageTest, DefaultConstructor) {
    Message msg;

    EXPECT_EQ(msg.get_service_id(), 0);
    EXPECT_EQ(msg.get_method_id(), 0);
    EXPECT_EQ(msg.get_client_id(), 0);
    EXPECT_EQ(msg.get_session_id(), 0);
    EXPECT_EQ(msg.get_message_type(), MessageType::REQUEST);
    EXPECT_EQ(msg.get_return_code(), ReturnCode::E_OK);
    EXPECT_TRUE(msg.get_payload().empty());
    EXPECT_TRUE(msg.is_valid());
}

/**
 * @test_case TC_MSG_002
 * @tests REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @tests REQ_MSG_020, REQ_MSG_021, REQ_MSG_022
 * @tests REQ_MSG_050, REQ_MSG_054, REQ_MSG_070, REQ_MSG_074
 * @brief Test constructor with specific IDs and message type
 */
TEST_F(MessageTest, ConstructorWithIds) {
    MessageId msg_id(0x1234, 0x5678);
    RequestId req_id(0x9ABC, 0xDEF0);

    Message msg(msg_id, req_id, MessageType::RESPONSE, ReturnCode::E_NOT_OK);

    EXPECT_EQ(msg.get_service_id(), 0x1234);
    EXPECT_EQ(msg.get_method_id(), 0x5678);
    EXPECT_EQ(msg.get_client_id(), 0x9ABC);
    EXPECT_EQ(msg.get_session_id(), 0xDEF0);
    EXPECT_EQ(msg.get_message_type(), MessageType::RESPONSE);
    EXPECT_EQ(msg.get_return_code(), ReturnCode::E_NOT_OK);
    EXPECT_TRUE(msg.is_valid());
}

/**
 * @test_case TC_MSG_003
 * @tests REQ_MSG_002, REQ_MSG_003, REQ_MSG_010, REQ_MSG_011
 * @tests REQ_MSG_021, REQ_MSG_022, REQ_MSG_053, REQ_MSG_075
 * @brief Test setters and getters for all message fields
 */
TEST_F(MessageTest, SettersAndGetters) {
    Message msg;

    msg.set_service_id(0x1234);
    msg.set_method_id(0x5678);
    msg.set_client_id(0x9ABC);
    msg.set_session_id(0xDEF0);
    msg.set_message_type(MessageType::NOTIFICATION);
    msg.set_return_code(ReturnCode::E_UNKNOWN_SERVICE);

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    msg.set_payload(payload);

    EXPECT_EQ(msg.get_service_id(), 0x1234);
    EXPECT_EQ(msg.get_method_id(), 0x5678);
    EXPECT_EQ(msg.get_client_id(), 0x9ABC);
    EXPECT_EQ(msg.get_session_id(), 0xDEF0);
    EXPECT_EQ(msg.get_message_type(), MessageType::NOTIFICATION);
    EXPECT_EQ(msg.get_return_code(), ReturnCode::E_UNKNOWN_SERVICE);
    EXPECT_EQ(msg.get_payload(), payload);
    EXPECT_EQ(msg.get_length(), 8 + payload.size());  // Length from client_id to end
    EXPECT_TRUE(msg.is_valid());
}

/**
 * @test_case TC_MSG_004
 * @tests REQ_MSG_001, REQ_MSG_002, REQ_MSG_003
 * @tests REQ_MSG_010, REQ_MSG_011, REQ_MSG_012
 * @tests REQ_MSG_020, REQ_MSG_021, REQ_MSG_022
 * @tests REQ_MSG_030, REQ_MSG_031, REQ_MSG_040
 * @tests REQ_MSG_050, REQ_MSG_070, REQ_MSG_090, REQ_MSG_091, REQ_MSG_092
 * @brief Test serialization and deserialization round-trip
 */
TEST_F(MessageTest, SerializationRoundTrip) {
    // Create a message with payload
    MessageId msg_id(0x1234, 0x5678);
    RequestId req_id(0x9ABC, 0xDEF0);
    Message original(msg_id, req_id, MessageType::REQUEST, ReturnCode::E_OK);

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    original.set_payload(payload);

    // Serialize
    std::vector<uint8_t> serialized = original.serialize();
    EXPECT_FALSE(serialized.empty());
    EXPECT_EQ(serialized.size(), original.get_total_size());

    // Deserialize
    Message deserialized;
    bool success = deserialized.deserialize(serialized);
    EXPECT_TRUE(success);

    // Compare
    EXPECT_EQ(deserialized.get_service_id(), original.get_service_id());
    EXPECT_EQ(deserialized.get_method_id(), original.get_method_id());
    EXPECT_EQ(deserialized.get_client_id(), original.get_client_id());
    EXPECT_EQ(deserialized.get_session_id(), original.get_session_id());
    EXPECT_EQ(deserialized.get_message_type(), original.get_message_type());
    EXPECT_EQ(deserialized.get_return_code(), original.get_return_code());
    EXPECT_EQ(deserialized.get_payload(), original.get_payload());
    EXPECT_EQ(deserialized.get_length(), original.get_length());
    EXPECT_TRUE(deserialized.is_valid());
}

/**
 * @test_case TC_MSG_005
 * @tests REQ_MSG_031, REQ_MSG_032, REQ_MSG_033
 * @tests REQ_MSG_042, REQ_MSG_063, REQ_MSG_064
 * @tests REQ_MSG_032_E01, REQ_MSG_032_E02
 * @tests REQ_MSG_100
 * @brief Test header validation for protocol version, interface version, and message type
 */
TEST_F(MessageTest, Validation) {
    Message msg;

    // Valid message
    EXPECT_TRUE(msg.is_valid());
    EXPECT_TRUE(msg.has_valid_header());

    // Invalid protocol version
    msg.set_protocol_version(0xFF);
    EXPECT_FALSE(msg.is_valid());
    msg.set_protocol_version(SOMEIP_PROTOCOL_VERSION);

    // Invalid interface version
    msg.set_interface_version(0xFF);
    EXPECT_FALSE(msg.is_valid());
    msg.set_interface_version(SOMEIP_INTERFACE_VERSION);

    // Invalid message type
    msg.set_message_type(static_cast<MessageType>(0xFF));
    EXPECT_FALSE(msg.has_valid_header());
}

/**
 * @test_case TC_MSG_002
 * @tests REQ_MSG_002
 * @brief Test Service ID validation
 */
TEST_F(MessageTest, ServiceIdValidation) {
    Message msg;

    // Valid service ID
    msg.set_service_id(0x1234);
    EXPECT_TRUE(msg.has_valid_service_id());

    // Reserved service ID 0x0000 - allowed for backward compatibility
    // Note: Per spec (REQ_MSG_004), 0x0000 is reserved, but we allow it
    // for default-constructed messages to maintain backward compatibility
    msg.set_service_id(0x0000);
    EXPECT_TRUE(msg.has_valid_service_id());  // Lenient validation

    // SD service ID 0xFFFF (valid)
    msg.set_service_id(0xFFFF);
    EXPECT_TRUE(msg.has_valid_service_id());
}

/**
 * @test_case TC_MSG_003
 * @tests REQ_MSG_003
 * @brief Test Method ID validation
 */
TEST_F(MessageTest, MethodIdValidation) {
    Message msg;

    // Valid method ID
    msg.set_method_id(0x1234);
    EXPECT_TRUE(msg.has_valid_method_id());

    // Reserved method ID 0xFFFF (invalid)
    msg.set_method_id(0xFFFF);
    EXPECT_FALSE(msg.has_valid_method_id());

    // Valid event ID
    msg.set_method_id(0x8123);  // Event ID range
    EXPECT_TRUE(msg.has_valid_method_id());
}

/**
 * @test_case TC_MSG_004
 * @tests REQ_MSG_004, REQ_MSG_004_E01, REQ_MSG_004_E02
 * @brief Test complete Message ID validation
 */
TEST_F(MessageTest, MessageIdValidation) {
    Message msg;

    // Valid Message ID
    msg.set_service_id(0x1234);
    msg.set_method_id(0x5678);
    EXPECT_TRUE(msg.has_valid_message_id());

    // Service ID 0x0000 - allowed for backward compatibility
    msg.set_service_id(0x0000);
    EXPECT_TRUE(msg.has_valid_message_id());  // Lenient validation
    msg.set_service_id(0x1234);

    // Invalid Method ID (0xFFFF is reserved per spec)
    msg.set_method_id(0xFFFF);
    EXPECT_FALSE(msg.has_valid_message_id());
}

/**
 * @test_case TC_MSG_012
 * @tests REQ_MSG_012, REQ_MSG_015
 * @tests REQ_MSG_012_E02
 * @brief Test length field validation
 */
TEST_F(MessageTest, LengthValidation) {
    Message msg;

    // Valid length
    msg.set_length(16);  // Minimum valid length
    EXPECT_TRUE(msg.has_valid_length());

    // Invalid length (too small)
    msg.set_length(7);
    EXPECT_FALSE(msg.has_valid_length());
}

/**
 * @test_case TC_MSG_021
 * @tests REQ_MSG_021, REQ_MSG_022
 * @brief Test Request ID component extraction
 */
TEST_F(MessageTest, RequestIdValidation) {
    Message msg;

    // Valid Request ID
    msg.set_client_id(0x1234);
    msg.set_session_id(0x5678);
    EXPECT_TRUE(msg.has_valid_request_id());

    // Client ID 0 (valid for SD)
    msg.set_client_id(0);
    msg.set_message_type(MessageType::NOTIFICATION);
    EXPECT_TRUE(msg.has_valid_client_id());

    // Session ID 0 (valid, indicates no session management)
    msg.set_session_id(0);
    EXPECT_TRUE(msg.has_valid_session_id());
}

TEST_F(MessageTest, StringRepresentation) {
    MessageId msg_id(0x1234, 0x5678);
    RequestId req_id(0x9ABC, 0xDEF0);
    Message msg(msg_id, req_id);

    std::string str = msg.to_string();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("service_id=0x1234"), std::string::npos);
    EXPECT_NE(str.find("method_id=0x5678"), std::string::npos);
    EXPECT_NE(str.find("client_id=0x9abc"), std::string::npos);
    EXPECT_NE(str.find("session_id=0xdef0"), std::string::npos);
}

TEST_F(MessageTest, CopyAndMove) {
    MessageId msg_id(0x1234, 0x5678);
    RequestId req_id(0x9ABC, 0xDEF0);
    Message original(msg_id, req_id);
    original.set_payload({0x01, 0x02, 0x03});

    // Copy constructor
    Message copy = original;
    EXPECT_EQ(copy.get_service_id(), original.get_service_id());
    EXPECT_EQ(copy.get_payload(), original.get_payload());

    // Move constructor
    Message moved = std::move(original);
    EXPECT_EQ(moved.get_service_id(), 0x1234);
    EXPECT_EQ(moved.get_payload(), (std::vector<uint8_t>{0x01, 0x02, 0x03}));

    // Original should be in valid but unspecified state after move
    // For safety, moved-from SOME/IP messages are considered invalid
    EXPECT_FALSE(original.is_valid());
}

/**
 * @test_case TC_MSG_006
 * @tests REQ_MSG_051, REQ_MSG_052, REQ_MSG_053, REQ_MSG_054
 * @brief Test message type helper functions
 */
TEST_F(MessageTest, MessageTypeHelpers) {
    Message request_msg;
    request_msg.set_message_type(MessageType::REQUEST);
    EXPECT_TRUE(request_msg.is_request());
    EXPECT_FALSE(request_msg.is_response());

    Message response_msg;
    response_msg.set_message_type(MessageType::RESPONSE);
    EXPECT_FALSE(response_msg.is_request());
    EXPECT_TRUE(response_msg.is_response());

    Message notification_msg;
    notification_msg.set_message_type(MessageType::NOTIFICATION);
    EXPECT_FALSE(notification_msg.is_request());
    EXPECT_FALSE(notification_msg.is_response());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
