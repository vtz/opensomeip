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
#include <cstddef>
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

/**
 * @test_case TC_E2E_001
 * @tests REQ_E2E_PLUGIN_005
 * @tests feat_req_someip_102
 * @tests feat_req_someip_103
 * @brief Test E2E header serialization/deserialization
 */
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

/**
 * @test_case TC_E2E_002
 * @tests REQ_E2E_PLUGIN_004
 * @brief Test CRC calculation - SAE-J1850
 */
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

/**
 * @test_case TC_E2E_003
 * @tests REQ_E2E_PLUGIN_004
 * @brief Test CRC calculation - ITU-T X.25
 */
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

    // Deserialize — receiver knows this message carries E2E protection
    Message deserialized;
    EXPECT_TRUE(deserialized.deserialize(serialized, /*expect_e2e=*/true));

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

// ============================================================================
// E2E CRC — MC/DC and edge cases
//
// calculate_crc has a switch on crc_type:
//   case 0 → SAE-J1850 (8-bit)
//   case 1 → ITU-T X.25 (16-bit)
//   case 2 → CRC32
//   default → 0
// And a guard: if (offset + length > data.size()) return 0
// ============================================================================

/**
 * @test_case TC_E2E_CRC_001
 * @tests REQ_E2E_PLUGIN_004
 * @brief calculate_crc: out-of-bounds range returns 0
 */
TEST_F(E2ETest, CRC_OutOfBoundsRange) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc = E2ECRC::calculate_crc(data, 2, 5, 0);  // offset+length=7 > size=4
    EXPECT_EQ(crc, 0u);
}

/**
 * @test_case TC_E2E_CRC_001b
 * @tests REQ_E2E_PLUGIN_004
 * @brief calculate_crc: size_t overflow in offset+length is caught
 */
TEST_F(E2ETest, CRC_OverflowGuard) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc = E2ECRC::calculate_crc(data, SIZE_MAX, 1, 0);
    EXPECT_EQ(crc, 0u);
}

/**
 * @test_case TC_E2E_CRC_002
 * @tests REQ_E2E_PLUGIN_004
 * @brief calculate_crc: all CRC type branches produce correct dispatch
 */
TEST_F(E2ETest, CRC_AllTypeBranches) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};

    uint32_t crc8  = E2ECRC::calculate_crc(data, 0, 4, 0);
    uint32_t crc16 = E2ECRC::calculate_crc(data, 0, 4, 1);
    uint32_t crc32 = E2ECRC::calculate_crc(data, 0, 4, 2);
    uint32_t unk   = E2ECRC::calculate_crc(data, 0, 4, 255);

    // Ranged CRC should match direct calls
    EXPECT_EQ(crc8,  static_cast<uint32_t>(E2ECRC::calculate_crc8_sae_j1850(data)));
    EXPECT_EQ(crc16, static_cast<uint32_t>(E2ECRC::calculate_crc16_itu_x25(data)));
    EXPECT_EQ(crc32, E2ECRC::calculate_crc32(data));
    EXPECT_EQ(unk, 0u);  // Unknown type returns 0
}

/**
 * @test_case TC_E2E_CRC_003
 * @tests REQ_E2E_PLUGIN_004
 * @brief CRC-8 SAE-J1850: deterministic for same input
 */
TEST_F(E2ETest, CRC8_Deterministic) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};

    uint8_t crc_a = E2ECRC::calculate_crc8_sae_j1850(data);
    uint8_t crc_b = E2ECRC::calculate_crc8_sae_j1850(data);

    EXPECT_EQ(crc_a, crc_b);
}

/**
 * @test_case TC_E2E_CRC_004
 * @tests REQ_E2E_PLUGIN_004
 * @brief CRC-16 ITU-T X.25: single-byte input
 */
TEST_F(E2ETest, CRC16_SingleByte) {
    std::vector<uint8_t> data = {0x42};
    uint16_t crc = E2ECRC::calculate_crc16_itu_x25(data);
    EXPECT_NE(crc, 0u);
    EXPECT_NE(crc, 0xFFFFu);
}

/**
 * @test_case TC_E2E_CRC_005
 * @tests REQ_E2E_PLUGIN_004
 * @brief Each CRC type produces a non-zero result for a known payload
 */
TEST_F(E2ETest, CRC_AllTypesNonZeroForKnownPayload) {
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};

    EXPECT_NE(E2ECRC::calculate_crc8_sae_j1850(data), 0u);
    EXPECT_NE(E2ECRC::calculate_crc16_itu_x25(data), 0u);
    EXPECT_NE(E2ECRC::calculate_crc32(data), 0u);
}

/**
 * @test_case TC_E2E_CRC_006
 * @tests REQ_E2E_PLUGIN_004
 * @brief calculate_crc with sub-range of data
 */
TEST_F(E2ETest, CRC_SubRange) {
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    // CRC over [2..4) = {0xCC, 0xDD}
    uint32_t crc_sub = E2ECRC::calculate_crc(data, 2, 2, 0);
    std::vector<uint8_t> sub = {0xCC, 0xDD};
    uint32_t crc_direct = E2ECRC::calculate_crc8_sae_j1850(sub);
    EXPECT_EQ(crc_sub, crc_direct);
}

// ============================================================================
// E2E Header deserialization edge cases
// ============================================================================

/**
 * @test_case TC_E2E_HDR_001
 * @tests REQ_E2E_PLUGIN_005
 * @brief Deserialization fails when buffer is too short
 */
TEST_F(E2ETest, HeaderDeserialize_BufferTooShort) {
    E2EHeader header;
    std::vector<uint8_t> short_buf(8, 0x00);
    EXPECT_FALSE(header.deserialize(short_buf));
}

/**
 * @test_case TC_E2E_HDR_002
 * @tests REQ_E2E_PLUGIN_005
 * @brief Deserialization with non-zero offset
 */
TEST_F(E2ETest, HeaderDeserialize_WithOffset) {
    E2EHeader original(0xAABBCCDD, 0x11223344, 0x5566, 0x7788);
    std::vector<uint8_t> serialized = original.serialize();

    // Prefix with garbage, use offset to skip it
    std::vector<uint8_t> padded = {0xFF, 0xFF, 0xFF, 0xFF};
    padded.insert(padded.end(), serialized.begin(), serialized.end());

    E2EHeader deserialized;
    EXPECT_TRUE(deserialized.deserialize(padded, 4));
    EXPECT_EQ(deserialized.crc, original.crc);
    EXPECT_EQ(deserialized.counter, original.counter);
    EXPECT_EQ(deserialized.data_id, original.data_id);
    EXPECT_EQ(deserialized.freshness_value, original.freshness_value);
}

/**
 * @test_case TC_E2E_HDR_003
 * @tests REQ_E2E_PLUGIN_005
 * @brief Deserialization fails when offset puts header past end of buffer
 */
TEST_F(E2ETest, HeaderDeserialize_OffsetPastEnd) {
    E2EHeader header;
    std::vector<uint8_t> buf(12, 0x00);
    EXPECT_FALSE(header.deserialize(buf, 8));  // offset 8 + header 12 = 20 > 12
}

/**
 * @test_case TC_E2E_HDR_004
 * @tests REQ_E2E_PLUGIN_005
 * @brief Header is_valid always returns true (profile-level validation)
 */
TEST_F(E2ETest, HeaderIsValid) {
    E2EHeader header;
    EXPECT_TRUE(header.is_valid());

    E2EHeader header2(0, 0, 0, 0);
    EXPECT_TRUE(header2.is_valid());

    E2EHeader header3(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF, 0xFFFF);
    EXPECT_TRUE(header3.is_valid());
}

/**
 * @test_case TC_E2E_HDR_005
 * @tests REQ_E2E_PLUGIN_005
 * @brief Header size is exactly 12 bytes
 */
TEST_F(E2ETest, HeaderSize) {
    EXPECT_EQ(E2EHeader::get_header_size(), 12u);
}

/**
 * @test_case TC_E2E_HDR_006
 * @tests REQ_E2E_PLUGIN_005
 * @brief Serialization round-trip preserves all field boundary values
 */
TEST_F(E2ETest, HeaderRoundTrip_BoundaryValues) {
    E2EHeader zero(0, 0, 0, 0);
    E2EHeader max_vals(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF, 0xFFFF);

    auto ser_zero = zero.serialize();
    auto ser_max = max_vals.serialize();
    EXPECT_EQ(ser_zero.size(), 12u);
    EXPECT_EQ(ser_max.size(), 12u);

    E2EHeader deser_zero, deser_max;
    EXPECT_TRUE(deser_zero.deserialize(ser_zero));
    EXPECT_TRUE(deser_max.deserialize(ser_max));

    EXPECT_EQ(deser_zero.crc, 0u);
    EXPECT_EQ(deser_zero.counter, 0u);
    EXPECT_EQ(deser_zero.data_id, 0u);
    EXPECT_EQ(deser_zero.freshness_value, 0u);

    EXPECT_EQ(deser_max.crc, 0xFFFFFFFFu);
    EXPECT_EQ(deser_max.counter, 0xFFFFFFFFu);
    EXPECT_EQ(deser_max.data_id, 0xFFFFu);
    EXPECT_EQ(deser_max.freshness_value, 0xFFFFu);
}

// ============================================================================
// E2E Protection — MC/DC for counter validation
//
// BasicE2EProfile::validate has these decisions for counter:
//   D1: last_counter == 0  (first message)
//   D2: header.counter >= 1 && header.counter <= max_counter_value
//   D3: header.counter == last_counter  (same message)
//   D4: header.counter > last_counter  (monotonic increase)
//   D5: last_counter > max_counter_value - 10  (near rollover)
//   D6: header.counter >= 1 && header.counter <= 10  (valid rollover)
//
// And for CRC type masking:
//   crc_type == 0 → mask 0xFF
//   crc_type == 1 → mask 0xFFFF
//   else → 32-bit compare
// ============================================================================

/**
 * @test_case TC_E2E_MCDC_001
 * @tests REQ_E2E_PLUGIN_001, REQ_E2E_PLUGIN_004
 * @brief E2E protect/validate with CRC type 0 (SAE-J1850, 8-bit)
 */
TEST_F(E2ETest, ProtectValidate_CRCType0) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0xAAAA);
    config.enable_crc = true;
    config.enable_counter = false;
    config.enable_freshness = false;
    config.crc_type = 0;  // SAE-J1850

    EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);
    EXPECT_EQ(protection.validate(msg, config), Result::SUCCESS);
}

/**
 * @test_case TC_E2E_MCDC_002
 * @tests REQ_E2E_PLUGIN_001, REQ_E2E_PLUGIN_004
 * @brief E2E protect/validate with CRC type 2 (CRC-32)
 */
TEST_F(E2ETest, ProtectValidate_CRCType2) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0xBBBB);
    config.enable_crc = true;
    config.enable_counter = false;
    config.enable_freshness = false;
    config.crc_type = 2;  // CRC-32

    EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);
    EXPECT_EQ(protection.validate(msg, config), Result::SUCCESS);
}

/**
 * @test_case TC_E2E_MCDC_003
 * @tests REQ_E2E_PLUGIN_001
 * @brief MC/DC: counter only, no CRC, no freshness — isolate counter logic
 *
 * D1: first message (last_counter==0) → counter_valid depends on D2
 * D2: header.counter >= 1 && header.counter <= max → accept
 */
TEST_F(E2ETest, CounterOnly_FirstMessage_ValidCounter) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02});

    E2EConfig config(0x1111);
    config.enable_crc = false;
    config.enable_counter = true;
    config.enable_freshness = false;
    config.max_counter_value = 100;

    EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);
    EXPECT_EQ(protection.validate(msg, config), Result::SUCCESS);
}

/**
 * @test_case TC_E2E_MCDC_004
 * @tests REQ_E2E_PLUGIN_001
 * @brief MC/DC: monotonically increasing counters are accepted (D4 path)
 */
TEST_F(E2ETest, CounterOnly_MonotonicIncrease) {
    E2EProtection protection;

    E2EConfig config(0x2222);
    config.enable_crc = false;
    config.enable_counter = true;
    config.enable_freshness = false;
    config.max_counter_value = 1000;

    for (int i = 0; i < 5; ++i) {
        Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
        msg.set_payload({0x01, 0x02});

        EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);
        EXPECT_EQ(protection.validate(msg, config), Result::SUCCESS)
            << "Message " << i << " should validate with increasing counter";
    }
}

/**
 * @test_case TC_E2E_MCDC_005
 * @tests REQ_E2E_PLUGIN_001
 * @brief MC/DC for enable_crc=F, enable_counter=F, enable_freshness=F
 *        All feature guards are false → only data_id checked.
 */
TEST_F(E2ETest, AllFeaturesDisabled) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02});

    E2EConfig config(0x3333);
    config.enable_crc = false;
    config.enable_counter = false;
    config.enable_freshness = false;

    EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);
    EXPECT_EQ(protection.validate(msg, config), Result::SUCCESS);
}

/**
 * @test_case TC_E2E_MCDC_006
 * @tests REQ_E2E_PLUGIN_001
 * @brief MC/DC: validate fails when no E2E header is present
 */
TEST_F(E2ETest, Validate_NoHeader) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02});

    E2EConfig config(0x4444);
    config.enable_crc = false;
    config.enable_counter = false;
    config.enable_freshness = false;

    Result result = protection.validate(msg, config);
    EXPECT_EQ(result, Result::INVALID_ARGUMENT);
}

/**
 * @test_case TC_E2E_MCDC_007
 * @tests REQ_E2E_PLUGIN_001
 * @brief MC/DC: CRC corruption with 8-bit CRC type — masked comparison
 */
TEST_F(E2ETest, CRCType0_Corruption) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02, 0x03, 0x04});

    E2EConfig config(0x5555);
    config.enable_crc = true;
    config.enable_counter = false;
    config.enable_freshness = false;
    config.crc_type = 0;

    EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);

    auto header_opt = msg.get_e2e_header();
    ASSERT_TRUE(header_opt.has_value());
    auto header = header_opt.value();
    header.crc ^= 0x01;  // Flip one bit in the 8-bit CRC
    msg.set_e2e_header(header);

    EXPECT_EQ(protection.validate(msg, config), Result::INVALID_ARGUMENT);
}

/**
 * @test_case TC_E2E_MCDC_008
 * @tests REQ_E2E_PLUGIN_001
 * @brief extract_header returns the E2E header when present
 */
TEST_F(E2ETest, ExtractHeader) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02});

    E2EConfig config(0x6666);
    config.enable_crc = true;
    config.enable_counter = true;
    config.enable_freshness = true;

    EXPECT_EQ(protection.protect(msg, config), Result::SUCCESS);

    auto extracted = protection.extract_header(msg);
    ASSERT_TRUE(extracted.has_value());
    EXPECT_EQ(extracted->data_id, 0x6666);
    EXPECT_NE(extracted->counter, 0u);
}

/**
 * @test_case TC_E2E_MCDC_009
 * @tests REQ_E2E_PLUGIN_001
 * @brief extract_header returns empty when no E2E header
 */
TEST_F(E2ETest, ExtractHeader_NoHeader) {
    E2EProtection protection;
    Message msg(MessageId(0x1234, 0x5678), RequestId(0x9ABC, 0xDEF0));
    msg.set_payload({0x01, 0x02});

    auto extracted = protection.extract_header(msg);
    EXPECT_FALSE(extracted.has_value());
}

/**
 * @test_case TC_E2E_MCDC_010
 * @tests REQ_E2E_PLUGIN_004
 * @brief Profile registry: default profile has expected identity
 */
TEST_F(E2ETest, ProfileRegistry_DefaultProfile) {
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    E2EProfile* profile = registry.get_default_profile();

    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->get_profile_id(), 0u);
    EXPECT_EQ(profile->get_profile_name(), "basic");
    EXPECT_EQ(profile->get_header_size(), E2EHeader::get_header_size());
}

/**
 * @test_case TC_E2E_MCDC_011
 * @tests REQ_E2E_PLUGIN_004
 * @brief Profile lookup by name matches lookup by ID
 */
TEST_F(E2ETest, ProfileRegistry_LookupByName) {
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    E2EProfile* by_id = registry.get_profile(static_cast<uint32_t>(0));
    E2EProfile* by_name = registry.get_profile(std::string("basic"));

    ASSERT_NE(by_id, nullptr);
    ASSERT_NE(by_name, nullptr);
    EXPECT_EQ(by_id, by_name);
}

/**
 * @test_case TC_E2E_MCDC_012
 * @tests REQ_E2E_PLUGIN_004
 * @brief Profile lookup returns nullptr for unknown profile
 */
TEST_F(E2ETest, ProfileRegistry_UnknownProfile) {
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    EXPECT_EQ(registry.get_profile(static_cast<uint32_t>(999)), nullptr);
    EXPECT_EQ(registry.get_profile(std::string("nonexistent")), nullptr);
}
