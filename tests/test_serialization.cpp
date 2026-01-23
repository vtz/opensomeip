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
#include <cmath>
#include <limits>
#include "serialization/serializer.h"

using namespace someip::serialization;

/**
 * @brief SOME/IP Serialization unit tests
 * @tests REQ_ARCH_001
 * @tests REQ_SER_001, REQ_SER_002, REQ_SER_003, REQ_SER_004
 * @tests REQ_SER_005, REQ_SER_006, REQ_SER_007, REQ_SER_008
 * @tests REQ_SER_010, REQ_SER_011, REQ_SER_012, REQ_SER_013, REQ_SER_014, REQ_SER_015, REQ_SER_016, REQ_SER_017
 * @tests REQ_SER_020, REQ_SER_021, REQ_SER_022, REQ_SER_023, REQ_SER_024
 * @tests REQ_SER_030, REQ_SER_031, REQ_SER_032, REQ_SER_033, REQ_SER_034, REQ_SER_035
 * @tests REQ_SER_040, REQ_SER_041, REQ_SER_042, REQ_SER_043, REQ_SER_044, REQ_SER_045, REQ_SER_046, REQ_SER_047
 * @tests REQ_SER_050, REQ_SER_051, REQ_SER_052, REQ_SER_053, REQ_SER_054, REQ_SER_055, REQ_SER_056
 * @tests REQ_SER_060, REQ_SER_061, REQ_SER_062, REQ_SER_063
 * @tests REQ_SER_070, REQ_SER_071, REQ_SER_072, REQ_SER_073, REQ_SER_074, REQ_SER_075
 * @tests REQ_SER_080, REQ_SER_081, REQ_SER_082
 * @tests REQ_SER_001_E01, REQ_SER_002_E01, REQ_SER_003_E01, REQ_SER_004_E01
 * @tests REQ_SER_005_E01, REQ_SER_006_E01, REQ_SER_007_E01, REQ_SER_008_E01
 * @tests REQ_SER_020_E01, REQ_SER_022_E01, REQ_SER_030_E01, REQ_SER_031_E01
 * @tests REQ_SER_032_E01, REQ_SER_033_E01, REQ_SER_040_E01, REQ_SER_043_E01
 * @tests REQ_SER_046_E01, REQ_SER_047_E01, REQ_SER_047_E02
 * @tests REQ_SER_050_E01, REQ_SER_050_E02, REQ_SER_053_E01, REQ_SER_055_E01
 * @tests REQ_SER_060_E01, REQ_SER_060_E02, REQ_SER_070_E01, REQ_SER_070_E02
 * @tests feat_req_someip_600
 * @tests feat_req_someip_601
 * @tests feat_req_someip_602
 * @tests feat_req_someip_610
 * @tests feat_req_someip_611
 * @tests feat_req_someip_231
 */

// Helper macro for deserialization result testing
#define EXPECT_DESERIALIZE_SUCCESS(result_expr, expected_value) \
    do { \
        auto result = (result_expr); \
        EXPECT_TRUE(result.is_success()) << "Deserialization failed"; \
        if (result.is_success()) { \
            EXPECT_EQ(result.get_value(), (expected_value)); \
        } \
    } while (0); // Note: semicolon is required after this macro

#define EXPECT_DESERIALIZE_ERROR(result_expr, expected_error) \
    do { \
        auto result = (result_expr); \
        EXPECT_TRUE(result.is_error()); \
        if (result.is_error()) { \
            EXPECT_EQ(result.get_error(), (expected_error)); \
        } \
    } while (0)

class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

/**
 * @test_case TC_SER_001
 * @tests REQ_SER_020, REQ_SER_021
 * @brief Test boolean serialization and deserialization
 */
TEST_F(SerializationTest, SerializeDeserializeBool) {
    Serializer serializer;
    Deserializer deserializer({});

    // Test true
    serializer.reset();
    serializer.serialize_bool(true);
    deserializer = Deserializer(serializer.get_buffer());
    auto result_true = deserializer.deserialize_bool();
    EXPECT_TRUE(result_true.is_success());
    EXPECT_TRUE(result_true.get_value());

    // Test false
    serializer.reset();
    serializer.serialize_bool(false);
    deserializer = Deserializer(serializer.get_buffer());
    auto result_false = deserializer.deserialize_bool();
    EXPECT_TRUE(result_false.is_success());
    EXPECT_FALSE(result_false.get_value());
}

/**
 * @test_case TC_SER_002
 * @tests REQ_SER_001, REQ_SER_005
 * @brief Test uint8 serialization and deserialization
 */
TEST_F(SerializationTest, SerializeDeserializeUint8) {
    Serializer serializer;
    Deserializer deserializer({});

    uint8_t test_values[] = {0, 1, 127, 255};

    for (uint8_t value : test_values) {
        serializer.reset();
        serializer.serialize_uint8(value);
        deserializer = Deserializer(serializer.get_buffer());
        auto result = deserializer.deserialize_uint8();
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.get_value(), value);
    }
}

/**
 * @test_case TC_SER_003
 * @tests REQ_SER_002, REQ_SER_006
 * @brief Test uint16 serialization and deserialization in Big Endian
 */
TEST_F(SerializationTest, SerializeDeserializeUint16) {
    Serializer serializer;
    Deserializer deserializer({});

    uint16_t test_values[] = {0, 1, 255, 65535, 12345};

    for (uint16_t value : test_values) {
        serializer.reset();
        serializer.serialize_uint16(value);
        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint16(), value);
    }
}

/**
 * @test_case TC_SER_004
 * @tests REQ_SER_003, REQ_SER_007
 * @brief Test uint32 serialization and deserialization in Big Endian
 */
TEST_F(SerializationTest, SerializeDeserializeUint32) {
    Serializer serializer;
    Deserializer deserializer({});

    uint32_t test_values[] = {0, 1, 65535, 4294967295U, 123456789};

    for (uint32_t value : test_values) {
        serializer.reset();
        serializer.serialize_uint32(value);
        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), value);
    }
}

/**
 * @test_case TC_SER_005
 * @tests REQ_SER_040, REQ_SER_041, REQ_SER_042, REQ_SER_043, REQ_SER_044, REQ_SER_045
 * @brief Test string serialization with length prefix and padding
 */
TEST_F(SerializationTest, SerializeDeserializeString) {
    Serializer serializer;
    Deserializer deserializer({});

    std::string test_strings[] = {"", "hello", "world", "some/ip test string"};

    for (const std::string& str : test_strings) {
        serializer.reset();
        serializer.serialize_string(str);
        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_string(), str);
    }
}

/**
 * @test_case TC_SER_006
 * @tests REQ_SER_060, REQ_SER_061, REQ_SER_062, REQ_SER_063
 * @brief Test array serialization with length prefix
 */
TEST_F(SerializationTest, SerializeDeserializeArray) {
    Serializer serializer;
    Deserializer deserializer({});

    std::vector<uint32_t> test_array = {1, 2, 3, 4, 5};

    serializer.reset();
    serializer.serialize_array(test_array);
    deserializer = Deserializer(serializer.get_buffer());

    // First read the array length that was serialized
    auto length_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(length_result.is_success());
    EXPECT_EQ(length_result.get_value(), test_array.size());
    uint32_t array_length = length_result.get_value();

    // Then read the array elements
    auto array_result = deserializer.deserialize_array<uint32_t>(array_length);
    EXPECT_TRUE(array_result.is_success());
    auto result_array = array_result.get_value();
    EXPECT_EQ(result_array.size(), test_array.size());
    for (size_t i = 0; i < test_array.size(); ++i) {
        EXPECT_EQ(result_array[i], test_array[i]);
    }
}

TEST_F(SerializationTest, ComplexSerialization) {
    Serializer serializer;

    // Serialize complex data
    serializer.serialize_uint32(0x12345678);
    serializer.serialize_string("SOME/IP");
    serializer.serialize_bool(true);
    serializer.serialize_uint16(0xABCD);

    // Verify buffer is not empty
    EXPECT_FALSE(serializer.get_buffer().empty());

    // Deserialize and verify
    Deserializer deserializer(serializer.get_buffer());

    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0x12345678U);
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_string(), "SOME/IP");
    auto bool_result = deserializer.deserialize_bool();
    EXPECT_TRUE(bool_result.is_success());
    EXPECT_TRUE(bool_result.get_value());
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint16(), 0xABCD);
}

/**
 * @test_case TC_SER_007
 * @tests REQ_SER_004, REQ_SER_008
 * @brief Test uint64 serialization and deserialization in Big Endian
 */
TEST_F(SerializationTest, SerializeDeserializeUint64) {
    Serializer serializer;
    Deserializer deserializer({});

    // Test boundary values and typical cases
    uint64_t test_values[] = {
        0ULL,                           // Minimum
        1ULL,
        0xFFFFFFFFULL,                  // Max uint32
        0x100000000ULL,                 // Just above uint32 max
        0xFFFFFFFFFFFFFFFFULL,          // Maximum uint64
        0x123456789ABCDEF0ULL,          // Arbitrary large value
        0x8000000000000000ULL           // High bit set (sign bit in signed)
    };

    for (uint64_t value : test_values) {
        serializer.reset();
        serializer.serialize_uint64(value);

        // Verify buffer size is exactly 8 bytes
        EXPECT_EQ(serializer.get_buffer().size(), 8u);

        deserializer = Deserializer(serializer.get_buffer());
        auto result = deserializer.deserialize_uint64();
        EXPECT_TRUE(result.is_success()) << "Failed for value: 0x" << std::hex << value;
        if (result.is_success()) {
            EXPECT_EQ(result.get_value(), value);
        }
    }
}

/**
 * @test_case TC_SER_008
 * @tests REQ_SER_010, REQ_SER_014
 * @brief Test int8 serialization (two's complement)
 */
TEST_F(SerializationTest, SerializeDeserializeInt8) {
    Serializer serializer;
    Deserializer deserializer({});

    int8_t test_values[] = {
        0,                  // Zero
        1, -1,              // Small values
        127,                // Max positive
        -128                // Min negative
    };

    for (int8_t value : test_values) {
        serializer.reset();
        serializer.serialize_int8(value);

        EXPECT_EQ(serializer.get_buffer().size(), 1u);

        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_int8(), value);
    }
}

/**
 * @test_case TC_SER_009
 * @tests REQ_SER_011, REQ_SER_015
 * @brief Test int16 serialization in Big Endian (two's complement)
 */
/**
 * @test_case TC_SER_015
 * @tests REQ_SER_015
 * @brief Test int16 serialization in Big Endian (two's complement)
 */
TEST_F(SerializationTest, SerializeDeserializeInt16) {
    Serializer serializer;
    Deserializer deserializer({});

    int16_t test_values[] = {
        0,                  // Zero
        1, -1,              // Small values
        32767,              // Max positive (0x7FFF)
        -32768,             // Min negative (0x8000)
        12345, -12345       // Arbitrary values
    };

    for (int16_t value : test_values) {
        serializer.reset();
        serializer.serialize_int16(value);

        EXPECT_EQ(serializer.get_buffer().size(), 2u);

        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_int16(), value);
    }
}

/**
 * @test_case TC_SER_016
 * @tests REQ_SER_016
 * @brief Test int32 serialization in Big Endian (two's complement)
 */
TEST_F(SerializationTest, SerializeDeserializeInt32) {
    Serializer serializer;
    Deserializer deserializer({});

    int32_t test_values[] = {
        0,                          // Zero
        1, -1,                      // Small values
        2147483647,                 // Max positive (0x7FFFFFFF)
        -2147483648,                // Min negative (0x80000000)
        123456789, -123456789       // Arbitrary values
    };

    for (int32_t value : test_values) {
        serializer.reset();
        serializer.serialize_int32(value);

        EXPECT_EQ(serializer.get_buffer().size(), 4u);

        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_int32(), value);
    }
}

/**
 * @test_case TC_SER_017
 * @tests REQ_SER_017
 * @brief Test int64 serialization in Big Endian (two's complement)
 */
TEST_F(SerializationTest, SerializeDeserializeInt64) {
    Serializer serializer;
    Deserializer deserializer({});

    int64_t test_values[] = {
        0LL,                                    // Zero
        1LL, -1LL,                              // Small values
        9223372036854775807LL,                  // Max positive
        -9223372036854775807LL - 1,             // Min negative
        123456789012345LL, -123456789012345LL   // Arbitrary values
    };

    for (int64_t value : test_values) {
        serializer.reset();
        serializer.serialize_int64(value);

        EXPECT_EQ(serializer.get_buffer().size(), 8u);

        deserializer = Deserializer(serializer.get_buffer());
        EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_int64(), value);
    }
}

/**
 * @test_case TC_SER_012
 * @tests REQ_SER_030, REQ_SER_032
 * @brief Test float serialization using IEEE 754 in Big Endian
 */
TEST_F(SerializationTest, SerializeDeserializeFloat) {
    Serializer serializer;
    Deserializer deserializer({});

    float test_values[] = {
        0.0f,                       // Zero
        1.0f, -1.0f,                // Unit values
        3.14159265f,                // Pi
        1.175494e-38f,              // Near min positive normal
        3.402823e+38f,              // Near max positive
        -3.402823e+38f,             // Near max negative
        0.000001f,                  // Small positive
        123456.789f                 // Arbitrary
    };

    for (float value : test_values) {
        serializer.reset();
        serializer.serialize_float(value);

        // IEEE 754 float32 is 4 bytes
        EXPECT_EQ(serializer.get_buffer().size(), 4u);

        deserializer = Deserializer(serializer.get_buffer());
        auto float_result = deserializer.deserialize_float();
        EXPECT_TRUE(float_result.is_success());
        float result = float_result.get_value();

        // Use approximate comparison for floating point
        EXPECT_FLOAT_EQ(result, value) << "Failed for value: " << value;
    }
}

/**
 * @test_case TC_SER_013
 * @tests REQ_SER_030, REQ_SER_032, REQ_SER_034, REQ_SER_035
 * @brief Test float special values (infinity, NaN) serialization
 */
TEST_F(SerializationTest, SerializeDeserializeFloatSpecialValues) {
    Serializer serializer;
    Deserializer deserializer({});

    // Test special IEEE 754 values
    float positive_infinity = std::numeric_limits<float>::infinity();
    float negative_infinity = -std::numeric_limits<float>::infinity();
    float nan_value = std::numeric_limits<float>::quiet_NaN();

    // Positive infinity
    serializer.reset();
    serializer.serialize_float(positive_infinity);
    deserializer = Deserializer(serializer.get_buffer());
    auto pos_inf_result = deserializer.deserialize_float();
    EXPECT_TRUE(pos_inf_result.is_success());
    EXPECT_TRUE(std::isinf(pos_inf_result.get_value()));

    // Negative infinity
    serializer.reset();
    serializer.serialize_float(negative_infinity);
    deserializer = Deserializer(serializer.get_buffer());
    auto neg_inf_result = deserializer.deserialize_float();
    EXPECT_TRUE(neg_inf_result.is_success());
    EXPECT_TRUE(std::isinf(neg_inf_result.get_value()));
    EXPECT_LT(neg_inf_result.get_value(), 0);

    // NaN (note: NaN != NaN, so we check with isnan)
    serializer.reset();
    serializer.serialize_float(nan_value);
    deserializer = Deserializer(serializer.get_buffer());
    auto nan_result = deserializer.deserialize_float();
    EXPECT_TRUE(nan_result.is_success());
    EXPECT_TRUE(std::isnan(nan_result.get_value()));
}

/**
 * @test_case TC_SER_014
 * @tests REQ_SER_031, REQ_SER_033
 * @brief Test double serialization using IEEE 754 in Big Endian
 */
TEST_F(SerializationTest, SerializeDeserializeDouble) {
    Serializer serializer;
    Deserializer deserializer({});

    double test_values[] = {
        0.0,                            // Zero
        1.0, -1.0,                      // Unit values
        3.141592653589793,              // Pi with more precision
        2.2250738585072014e-308,        // Near min positive normal
        1.7976931348623157e+308,        // Near max positive
        -1.7976931348623157e+308,       // Near max negative
        0.000000000001,                 // Small positive
        123456789.123456789             // Arbitrary with precision
    };

    for (double value : test_values) {
        serializer.reset();
        serializer.serialize_double(value);

        // IEEE 754 float64 (double) is 8 bytes
        EXPECT_EQ(serializer.get_buffer().size(), 8u);

        deserializer = Deserializer(serializer.get_buffer());
        auto double_result = deserializer.deserialize_double();
        EXPECT_TRUE(double_result.is_success());
        double result = double_result.get_value();

        EXPECT_DOUBLE_EQ(result, value) << "Failed for value: " << value;
    }
}

TEST_F(SerializationTest, SerializeDeserializeDoubleSpecialValues) {
    Serializer serializer;
    Deserializer deserializer({});

    // Test special IEEE 754 values for double
    double positive_infinity = std::numeric_limits<double>::infinity();
    double negative_infinity = -std::numeric_limits<double>::infinity();
    double nan_value = std::numeric_limits<double>::quiet_NaN();

    // Positive infinity
    serializer.reset();
    serializer.serialize_double(positive_infinity);
    deserializer = Deserializer(serializer.get_buffer());
    auto pos_inf_result = deserializer.deserialize_double();
    EXPECT_TRUE(pos_inf_result.is_success());
    EXPECT_TRUE(std::isinf(pos_inf_result.get_value()));

    // Negative infinity
    serializer.reset();
    serializer.serialize_double(negative_infinity);
    deserializer = Deserializer(serializer.get_buffer());
    auto neg_inf_result = deserializer.deserialize_double();
    EXPECT_TRUE(neg_inf_result.is_success());
    EXPECT_TRUE(std::isinf(neg_inf_result.get_value()));
    EXPECT_LT(neg_inf_result.get_value(), 0);

    // NaN
    serializer.reset();
    serializer.serialize_double(nan_value);
    deserializer = Deserializer(serializer.get_buffer());
    auto nan_result = deserializer.deserialize_double();
    EXPECT_TRUE(nan_result.is_success());
    EXPECT_TRUE(std::isnan(nan_result.get_value()));
}

// =============================================================================
// UT-SER-011: Fixed-Length Array Serialization (feat_req_someip_241)
// =============================================================================
TEST_F(SerializationTest, SerializeDeserializeUint8Array) {
    Serializer serializer;
    Deserializer deserializer({});

    std::vector<uint8_t> test_array = {0x01, 0x02, 0x03, 0x04, 0x05, 0xFE, 0xFF};

    serializer.reset();
    serializer.serialize_array(test_array);

    deserializer = Deserializer(serializer.get_buffer());
    auto length_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(length_result.is_success());
    uint32_t length = length_result.get_value();
    EXPECT_EQ(length, test_array.size());

    auto array_result = deserializer.deserialize_array<uint8_t>(length);
    EXPECT_TRUE(array_result.is_success());
    auto result = array_result.get_value();
    EXPECT_EQ(result, test_array);
}

TEST_F(SerializationTest, SerializeDeserializeInt16Array) {
    Serializer serializer;
    Deserializer deserializer({});

    std::vector<int16_t> test_array = {-32768, -1, 0, 1, 32767, 12345};

    serializer.reset();
    serializer.serialize_array(test_array);

    deserializer = Deserializer(serializer.get_buffer());
    auto length_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(length_result.is_success());
    uint32_t length = length_result.get_value();
    EXPECT_EQ(length, test_array.size());

    auto array_result = deserializer.deserialize_array<int16_t>(length);
    EXPECT_TRUE(array_result.is_success());
    auto result = array_result.get_value();
    EXPECT_EQ(result, test_array);
}

TEST_F(SerializationTest, SerializeDeserializeFloatArray) {
    Serializer serializer;
    Deserializer deserializer({});

    std::vector<float> test_array = {0.0f, 1.0f, -1.0f, 3.14159f, 1000000.5f};

    serializer.reset();
    serializer.serialize_array(test_array);

    deserializer = Deserializer(serializer.get_buffer());
    auto length_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(length_result.is_success());
    uint32_t length = length_result.get_value();
    EXPECT_EQ(length, test_array.size());

    auto array_result = deserializer.deserialize_array<float>(length);
    EXPECT_TRUE(array_result.is_success());
    auto result = array_result.get_value();
    ASSERT_EQ(result.size(), test_array.size());
    for (size_t i = 0; i < test_array.size(); ++i) {
        EXPECT_FLOAT_EQ(result[i], test_array[i]);
    }
}

TEST_F(SerializationTest, SerializeDeserializeEmptyArray) {
    Serializer serializer;
    Deserializer deserializer({});

    std::vector<uint32_t> empty_array;

    serializer.reset();
    serializer.serialize_array(empty_array);

    deserializer = Deserializer(serializer.get_buffer());
    auto length_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(length_result.is_success());
    uint32_t length = length_result.get_value();
    EXPECT_EQ(length, 0u);

    auto array_result = deserializer.deserialize_array<uint32_t>(length);
    EXPECT_TRUE(array_result.is_success());
    auto result = array_result.get_value();
    EXPECT_TRUE(result.empty());
}

// =============================================================================
// UT-SER-012: Dynamic-Length Array with String Elements
// =============================================================================
TEST_F(SerializationTest, SerializeDeserializeStringArray) {
    Serializer serializer;
    Deserializer deserializer({});

    std::vector<std::string> test_array = {"hello", "world", "SOME/IP", ""};

    serializer.reset();
    serializer.serialize_array(test_array);

    deserializer = Deserializer(serializer.get_buffer());
    auto length_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(length_result.is_success());
    uint32_t length = length_result.get_value();
    EXPECT_EQ(length, test_array.size());

    auto array_result = deserializer.deserialize_array<std::string>(length);
    EXPECT_TRUE(array_result.is_success());
    auto result = array_result.get_value();
    EXPECT_EQ(result, test_array);
}

// =============================================================================
// Big-Endian Byte Order Verification (feat_req_someip_42)
// =============================================================================
TEST_F(SerializationTest, VerifyBigEndianUint16) {
    Serializer serializer;

    // 0x1234 should serialize as [0x12, 0x34] in big-endian
    serializer.serialize_uint16(0x1234);

    const auto& buffer = serializer.get_buffer();
    ASSERT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer[0], 0x12);  // High byte first
    EXPECT_EQ(buffer[1], 0x34);  // Low byte second
}

TEST_F(SerializationTest, VerifyBigEndianUint32) {
    Serializer serializer;

    // 0x12345678 should serialize as [0x12, 0x34, 0x56, 0x78] in big-endian
    serializer.serialize_uint32(0x12345678);

    const auto& buffer = serializer.get_buffer();
    ASSERT_EQ(buffer.size(), 4u);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
}

TEST_F(SerializationTest, VerifyBigEndianUint64) {
    Serializer serializer;

    // 0x0102030405060708 should serialize as [0x01, 0x02, ..., 0x08] in big-endian
    serializer.serialize_uint64(0x0102030405060708ULL);

    const auto& buffer = serializer.get_buffer();
    ASSERT_EQ(buffer.size(), 8u);
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[1], 0x02);
    EXPECT_EQ(buffer[2], 0x03);
    EXPECT_EQ(buffer[3], 0x04);
    EXPECT_EQ(buffer[4], 0x05);
    EXPECT_EQ(buffer[5], 0x06);
    EXPECT_EQ(buffer[6], 0x07);
    EXPECT_EQ(buffer[7], 0x08);
}

TEST_F(SerializationTest, VerifyBigEndianNegativeInt16) {
    Serializer serializer;

    // -1 (0xFFFF) should serialize as [0xFF, 0xFF] in big-endian
    serializer.serialize_int16(-1);

    const auto& buffer = serializer.get_buffer();
    ASSERT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[1], 0xFF);
}

/**
 * @test_case TC_SER_020
 * @tests REQ_SER_050, REQ_SER_051, REQ_SER_052
 * @brief Test 4-byte alignment with padding
 */
TEST_F(SerializationTest, AlignTo4Bytes) {
    Serializer serializer;

    // Serialize 1 byte, then align to 4
    serializer.serialize_uint8(0x12);
    EXPECT_EQ(serializer.get_size(), 1u);

    serializer.align_to(4);
    EXPECT_EQ(serializer.get_size(), 4u);  // Should be padded to 4 bytes

    // Verify padding is zeros
    const auto& buffer = serializer.get_buffer();
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x00);  // Padding
    EXPECT_EQ(buffer[2], 0x00);  // Padding
    EXPECT_EQ(buffer[3], 0x00);  // Padding
}

/**
 * @test_case TC_SER_021
 * @tests REQ_SER_050, REQ_SER_051, REQ_SER_052
 * @brief Test 8-byte alignment with padding
 */
TEST_F(SerializationTest, AlignTo8Bytes) {
    Serializer serializer;

    // Serialize 3 bytes, then align to 8
    serializer.serialize_uint8(0x01);
    serializer.serialize_uint8(0x02);
    serializer.serialize_uint8(0x03);
    EXPECT_EQ(serializer.get_size(), 3u);

    serializer.align_to(8);
    EXPECT_EQ(serializer.get_size(), 8u);  // Should be padded to 8 bytes
}

TEST_F(SerializationTest, AlignAlreadyAligned) {
    Serializer serializer;

    // Serialize 4 bytes, then align to 4 (should do nothing)
    serializer.serialize_uint32(0x12345678);
    EXPECT_EQ(serializer.get_size(), 4u);

    serializer.align_to(4);
    EXPECT_EQ(serializer.get_size(), 4u);  // No change
}

TEST_F(SerializationTest, DeserializerAlign) {
    Serializer serializer;

    // Create buffer: [0x12, pad, pad, pad, 0x56, 0x78, 0x9A, 0xBC]
    serializer.serialize_uint8(0x12);
    serializer.align_to(4);
    serializer.serialize_uint32(0x56789ABC);

    Deserializer deserializer(serializer.get_buffer());

    auto byte_result = deserializer.deserialize_uint8();
    EXPECT_TRUE(byte_result.is_success());
    uint8_t byte_val = byte_result.get_value();
    EXPECT_EQ(byte_val, 0x12);

    deserializer.align_to(4);  // Skip padding

    auto int_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(int_result.is_success());
    uint32_t int_val = int_result.get_value();
    EXPECT_EQ(int_val, 0x56789ABCu);
}

// =============================================================================
// Boolean Low Bit Only (feat_req_someip_817)
// =============================================================================
TEST_F(SerializationTest, BooleanUsesLowestBitOnly) {
    Serializer serializer;

    // True should serialize as 0x01
    serializer.serialize_bool(true);
    EXPECT_EQ(serializer.get_buffer()[0], 0x01);

    serializer.reset();

    // False should serialize as 0x00
    serializer.serialize_bool(false);
    EXPECT_EQ(serializer.get_buffer()[0], 0x00);
}

// =============================================================================
// Deserializer Position and Navigation Tests
// =============================================================================
TEST_F(SerializationTest, DeserializerPositionTracking) {
    Serializer serializer;
    serializer.serialize_uint32(0x12345678);
    serializer.serialize_uint16(0xABCD);
    serializer.serialize_uint8(0xFF);

    Deserializer deserializer(serializer.get_buffer());

    EXPECT_EQ(deserializer.get_position(), 0u);
    EXPECT_EQ(deserializer.get_remaining(), 7u);

    auto dummy32 = deserializer.deserialize_uint32();
    EXPECT_TRUE(dummy32.is_success());
    EXPECT_EQ(deserializer.get_position(), 4u);
    EXPECT_EQ(deserializer.get_remaining(), 3u);

    auto dummy16 = deserializer.deserialize_uint16();
    EXPECT_TRUE(dummy16.is_success());
    EXPECT_EQ(deserializer.get_position(), 6u);
    EXPECT_EQ(deserializer.get_remaining(), 1u);

    auto dummy8 = deserializer.deserialize_uint8();
    EXPECT_TRUE(dummy8.is_success());
    EXPECT_EQ(deserializer.get_position(), 7u);
    EXPECT_EQ(deserializer.get_remaining(), 0u);
}

TEST_F(SerializationTest, DeserializerSkip) {
    Serializer serializer;
    serializer.serialize_uint32(0x11111111);
    serializer.serialize_uint32(0x22222222);
    serializer.serialize_uint32(0x33333333);

    Deserializer deserializer(serializer.get_buffer());

    deserializer.skip(4);  // Skip first uint32
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0x22222222u);

    deserializer.skip(4);  // Skip last uint32
    EXPECT_EQ(deserializer.get_remaining(), 0u);
}

TEST_F(SerializationTest, DeserializerSetPosition) {
    Serializer serializer;
    serializer.serialize_uint32(0x11111111);
    serializer.serialize_uint32(0x22222222);
    serializer.serialize_uint32(0x33333333);

    Deserializer deserializer(serializer.get_buffer());

    // Jump to third uint32
    EXPECT_TRUE(deserializer.set_position(8));
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0x33333333u);

    // Jump back to first
    EXPECT_TRUE(deserializer.set_position(0));
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0x11111111u);

    // Invalid position should fail
    EXPECT_FALSE(deserializer.set_position(100));
}

TEST_F(SerializationTest, DeserializerReset) {
    Serializer serializer;
    serializer.serialize_uint32(0x12345678);

    Deserializer deserializer(serializer.get_buffer());

    auto reset_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(reset_result.is_success());
    EXPECT_EQ(deserializer.get_position(), 4u);

    deserializer.reset();
    EXPECT_EQ(deserializer.get_position(), 0u);
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0x12345678u);
}

// =============================================================================
// Complex Nested Serialization Scenario
// =============================================================================
TEST_F(SerializationTest, NestedDataStructure) {
    Serializer serializer;

    // Simulate a complex struct:
    // - uint16 id
    // - uint32 timestamp
    // - float temperature
    // - bool active
    // - string name
    // - array<uint8> data

    serializer.serialize_uint16(0x1234);                    // id
    serializer.serialize_uint32(1702500000);                // timestamp (unix)
    serializer.serialize_float(25.5f);                      // temperature
    serializer.serialize_bool(true);                        // active
    serializer.serialize_string("Sensor01");                // name
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD};
    serializer.serialize_array(data);                       // data

    // Deserialize and verify
    Deserializer deserializer(serializer.get_buffer());

    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint16(), 0x1234u);
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 1702500000u);
    auto float_result = deserializer.deserialize_float();
    EXPECT_TRUE(float_result.is_success());
    EXPECT_FLOAT_EQ(float_result.get_value(), 25.5f);
    auto bool_result = deserializer.deserialize_bool();
    EXPECT_TRUE(bool_result.is_success());
    EXPECT_TRUE(bool_result.get_value());
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_string(), "Sensor01");

    auto data_len_result = deserializer.deserialize_uint32();
    EXPECT_TRUE(data_len_result.is_success());
    EXPECT_EQ(data_len_result.get_value(), 4u);
    uint32_t data_len = data_len_result.get_value();
    auto array_result = deserializer.deserialize_array<uint8_t>(data_len);
    EXPECT_TRUE(array_result.is_success());
    auto result_data = array_result.get_value();
    EXPECT_EQ(result_data, data);

    EXPECT_EQ(deserializer.get_remaining(), 0u);
}

// =============================================================================
// Buffer Move Semantics Test
// =============================================================================
TEST_F(SerializationTest, MoveBuffer) {
    Serializer serializer;
    serializer.serialize_uint32(0x12345678);
    serializer.serialize_uint32(0xABCDEF01);

    // Get size before move
    size_t size_before = serializer.get_size();
    EXPECT_EQ(size_before, 8u);

    // Move buffer out
    std::vector<uint8_t> moved_buffer = serializer.move_buffer();

    EXPECT_EQ(moved_buffer.size(), 8u);

    // Verify contents
    Deserializer deserializer(moved_buffer);
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0x12345678u);
    EXPECT_DESERIALIZE_SUCCESS(deserializer.deserialize_uint32(), 0xABCDEF01u);
}

/**
 * @test_case TC_SER_030
 * @tests REQ_SER_005_E01, REQ_SER_006_E01, REQ_SER_007_E01, REQ_SER_008_E01
 * @tests REQ_SER_070, REQ_SER_071, REQ_SER_072
 * @brief Test deserialization error handling for buffer underflow
 */
TEST_F(SerializationTest, DeserializationErrorHandling) {
    // Test that errors can be distinguished from valid data
    Serializer serializer;
    serializer.serialize_bool(true);  // 1 byte of valid data

    std::vector<uint8_t> buffer = serializer.get_buffer();
    EXPECT_EQ(buffer.size(), 1u);

    Deserializer deserializer(buffer);

    // Valid deserialization should work
    auto bool_result = deserializer.deserialize_bool();
    EXPECT_TRUE(bool_result.is_success());
    EXPECT_TRUE(bool_result.get_value());

    // Attempting to read more data should fail with MALFORMED_MESSAGE
    auto error_result = deserializer.deserialize_uint32();  // Need 4 bytes, only 0 left
    EXPECT_TRUE(error_result.is_error());
    EXPECT_EQ(error_result.get_error(), someip::Result::MALFORMED_MESSAGE);

    // Demonstrate that this fixes the original issue:
    // Before: deserialize_bool() returned false for both "valid false" and "insufficient data"
    // After: We can distinguish between success with false value vs error
    Serializer false_serializer;
    false_serializer.serialize_bool(false);  // 1 byte: valid false

    Deserializer false_deserializer(false_serializer.get_buffer());
    auto false_result = false_deserializer.deserialize_bool();

    // This should be success with false value, not an error
    EXPECT_TRUE(false_result.is_success());
    EXPECT_FALSE(false_result.get_value());  // Valid false, not error

    // If we try to read more, we get error
    auto false_error = false_deserializer.deserialize_uint32();
    EXPECT_TRUE(false_error.is_error());
    EXPECT_EQ(false_error.get_error(), someip::Result::MALFORMED_MESSAGE);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
