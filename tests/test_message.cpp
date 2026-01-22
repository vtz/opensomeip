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
 * @tests REQ_ARCH_001
 * @tests REQ_ARCH_003
 * @tests feat_req_someip_538
 * @tests feat_req_someip_539
 * @tests feat_req_someip_540
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
