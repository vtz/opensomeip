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
#include "e2e/e2e_protection.h"
#include "e2e/e2e_header.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_crc.h"
#include "e2e/e2e_profile_registry.h"
#include "e2e/e2e_profiles/standard_profile.h"
#include "someip/message.h"
#include "common/result.h"

using namespace someip;
using namespace someip::e2e;

class E2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize basic profile (reference implementation)
        initialize_basic_profile();
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Test E2E header serialization/deserialization
TEST_F(E2ETest, HeaderSerialization) {
    E2EHeader header(0x12345678, 0xABCDEF00, 0x1234, 0x5678);

    std::vector<uint8_t> serialized = header.serialize();
    EXPECT_EQ(serialized.size(), E2EHeader::get_header_size());

    E2EHeader deserialized;
    EXPECT_TRUE(deserialized.deserialize(serialized));

    EXPECT_EQ(deserialized.crc, header.crc);
    EXPECT_EQ(deserialized.counter, header.counter);
    EXPECT_EQ(deserialized.data_id, header.data_id);
    EXPECT_EQ(deserialized.freshness_value, header.freshness_value);
}

// Test CRC calculation - SAE-J1850
TEST_F(E2ETest, CRC8SAEJ1850) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    uint8_t crc = E2ECRC::calculate_crc8_sae_j1850(data);

    // CRC should be non-zero for non-empty data
    EXPECT_NE(crc, 0);

    // Test with empty data
    std::vector<uint8_t> empty;
    uint8_t crc_empty = E2ECRC::calculate_crc8_sae_j1850(empty);
    EXPECT_EQ(crc_empty, 0xFF);  // SAE-J1850 init value
}

// Test CRC calculation - ITU-T X.25
TEST_F(E2ETest, CRC16ITUX25) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = E2ECRC::calculate_crc16_itu_x25(data);

    // CRC should be non-zero for non-empty data
    EXPECT_NE(crc, 0);

    // Test with empty data
    std::vector<uint8_t> empty;
    uint16_t crc_empty = E2ECRC::calculate_crc16_itu_x25(empty);
    EXPECT_EQ(crc_empty, 0xFFFF);  // ITU-T X.25 init value
}

// Test CRC calculation - CRC32
TEST_F(E2ETest, CRC32) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc = E2ECRC::calculate_crc32(data);

    // CRC should be non-zero for non-empty data
    EXPECT_NE(crc, 0);
}

// Test profile registry
TEST_F(E2ETest, ProfileRegistry) {
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();

    // Default profile should be registered
    E2EProfile* default_profile = registry.get_default_profile();
    ASSERT_NE(default_profile, nullptr);
    EXPECT_EQ(default_profile->get_profile_id(), 0);
    EXPECT_EQ(default_profile->get_profile_name(), "basic");
}

// Test E2E protection - protect message
TEST_F(E2ETest, ProtectMessage) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0x1234);
    config.enable_crc = true;
    config.enable_counter = true;
    config.enable_freshness = true;
    config.crc_type = 1;  // ITU-T X.25

    Result result = protection.protect(msg, config);
    EXPECT_EQ(result, Result::SUCCESS);
    EXPECT_TRUE(msg.has_e2e_header());

    auto header_opt = msg.get_e2e_header();
    ASSERT_TRUE(header_opt.has_value());
    EXPECT_EQ(header_opt->data_id, 0x1234);
    EXPECT_NE(header_opt->crc, 0);
    EXPECT_NE(header_opt->counter, 0);
}

// Test E2E protection - validate message
TEST_F(E2ETest, ValidateMessage) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0x1234);
    config.enable_crc = true;
    config.enable_counter = true;
    config.enable_freshness = true;
    config.crc_type = 1;  // ITU-T X.25

    // Protect message
    Result result = protection.protect(msg, config);
    EXPECT_EQ(result, Result::SUCCESS);

    // Validate message
    result = protection.validate(msg, config);
    EXPECT_EQ(result, Result::SUCCESS);
}

// Test E2E protection - invalid CRC
TEST_F(E2ETest, InvalidCRC) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0x1234);
    config.enable_crc = true;
    config.enable_counter = false;
    config.enable_freshness = false;
    config.crc_type = 1;  // ITU-T X.25

    // Protect message
    Result result = protection.protect(msg, config);
    EXPECT_EQ(result, Result::SUCCESS);

    // Corrupt the CRC
    auto header_opt = msg.get_e2e_header();
    ASSERT_TRUE(header_opt.has_value());
    E2EHeader corrupted_header = header_opt.value();
    corrupted_header.crc = 0xDEADBEEF;
    msg.set_e2e_header(corrupted_header);

    // Validation should fail
    result = protection.validate(msg, config);
    EXPECT_NE(result, Result::SUCCESS);
}

// Test E2E protection - wrong data ID
TEST_F(E2ETest, WrongDataID) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0x1234);
    config.enable_crc = true;
    config.enable_counter = false;
    config.enable_freshness = false;

    // Protect message
    Result result = protection.protect(msg, config);
    EXPECT_EQ(result, Result::SUCCESS);

    // Validate with wrong data ID
    E2EConfig wrong_config(0x5678);
    wrong_config.enable_crc = true;
    result = protection.validate(msg, wrong_config);
    EXPECT_NE(result, Result::SUCCESS);
}

// Test message serialization with E2E header
TEST_F(E2ETest, MessageSerializationWithE2E) {
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EHeader header(0x12345678, 0xABCDEF00, 0x1234, 0x5678);
    msg.set_e2e_header(header);

    // Serialize
    std::vector<uint8_t> serialized = msg.serialize();

    // Deserialize
    Message deserialized;
    EXPECT_TRUE(deserialized.deserialize(serialized));

    // Check E2E header
    EXPECT_TRUE(deserialized.has_e2e_header());
    auto header_opt = deserialized.get_e2e_header();
    ASSERT_TRUE(header_opt.has_value());
    EXPECT_EQ(header_opt->crc, header.crc);
    EXPECT_EQ(header_opt->counter, header.counter);
    EXPECT_EQ(header_opt->data_id, header.data_id);
    EXPECT_EQ(header_opt->freshness_value, header.freshness_value);
}

// Test message without E2E header
TEST_F(E2ETest, MessageWithoutE2E) {
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    EXPECT_FALSE(msg.has_e2e_header());

    E2EProtection protection;
    EXPECT_FALSE(protection.has_e2e_protection(msg));
}
