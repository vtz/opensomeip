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

/**
 * @file test_capi_serializer.cpp
 * @brief Unit tests for C API serializer/deserializer.
 */

#include <gtest/gtest.h>
#include "capi/opensomeip.h"
#include <cstring>

/**
 * @test_case TC_CAPI_SER_CREATE_001
 * @tests REQ_CAPI_003, REQ_CAPI_008
 */
TEST(CapiSerializer, CreateAndDestroy) {
    opensomeip_serializer_t* ser = nullptr;
    ASSERT_EQ(opensomeip_serializer_create(&ser), OPENSOMEIP_RESULT_SUCCESS);
    ASSERT_NE(ser, nullptr);
    EXPECT_EQ(opensomeip_serializer_destroy(ser), OPENSOMEIP_RESULT_SUCCESS);
}

/**
 * @test_case TC_CAPI_SER_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiSerializer, NullHandleReturnsError) {
    EXPECT_EQ(opensomeip_serializer_create(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_serializer_destroy(nullptr), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_serializer_write_uint8(nullptr, 0), OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}

/**
 * @test_case TC_CAPI_SER_ROUNDTRIP_001
 * @tests REQ_CAPI_008
 */
TEST(CapiSerializer, Uint8RoundTrip) {
    opensomeip_serializer_t* ser = nullptr;
    ASSERT_EQ(opensomeip_serializer_create(&ser), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_serializer_write_uint8(ser, 0x42), OPENSOMEIP_RESULT_SUCCESS);

    size_t sz = 0;
    EXPECT_EQ(opensomeip_serializer_get_size(ser, &sz), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(sz, 1u);

    uint8_t buf[16];
    size_t buf_len = sizeof(buf);
    EXPECT_EQ(opensomeip_serializer_get_data(ser, buf, &buf_len), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(buf_len, 1u);

    opensomeip_deserializer_t* de = nullptr;
    ASSERT_EQ(opensomeip_deserializer_create(&de, buf, buf_len), OPENSOMEIP_RESULT_SUCCESS);

    uint8_t val = 0;
    EXPECT_EQ(opensomeip_deserializer_read_uint8(de, &val), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(val, 0x42);

    opensomeip_deserializer_destroy(de);
    opensomeip_serializer_destroy(ser);
}

/**
 * @test_case TC_CAPI_SER_ROUNDTRIP_002
 * @tests REQ_CAPI_008
 */
TEST(CapiSerializer, MultiTypeRoundTrip) {
    opensomeip_serializer_t* ser = nullptr;
    ASSERT_EQ(opensomeip_serializer_create(&ser), OPENSOMEIP_RESULT_SUCCESS);

    EXPECT_EQ(opensomeip_serializer_write_uint16(ser, 0x1234), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(opensomeip_serializer_write_uint32(ser, 0xDEADBEEF), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(opensomeip_serializer_write_uint64(ser, 0x0102030405060708ULL), OPENSOMEIP_RESULT_SUCCESS);

    uint8_t buf[64];
    size_t buf_len = sizeof(buf);
    EXPECT_EQ(opensomeip_serializer_get_data(ser, buf, &buf_len), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(buf_len, 2u + 4u + 8u);

    opensomeip_deserializer_t* de = nullptr;
    ASSERT_EQ(opensomeip_deserializer_create(&de, buf, buf_len), OPENSOMEIP_RESULT_SUCCESS);

    uint16_t v16 = 0;
    EXPECT_EQ(opensomeip_deserializer_read_uint16(de, &v16), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(v16, 0x1234);

    uint32_t v32 = 0;
    EXPECT_EQ(opensomeip_deserializer_read_uint32(de, &v32), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(v32, 0xDEADBEEF);

    uint64_t v64 = 0;
    EXPECT_EQ(opensomeip_deserializer_read_uint64(de, &v64), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(v64, 0x0102030405060708ULL);

    size_t rem = 999;
    EXPECT_EQ(opensomeip_deserializer_get_remaining(de, &rem), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(rem, 0u);

    opensomeip_deserializer_destroy(de);
    opensomeip_serializer_destroy(ser);
}

/**
 * @test_case TC_CAPI_SER_BYTES_001
 * @tests REQ_CAPI_008
 */
TEST(CapiSerializer, BytesWriteRead) {
    opensomeip_serializer_t* ser = nullptr;
    ASSERT_EQ(opensomeip_serializer_create(&ser), OPENSOMEIP_RESULT_SUCCESS);

    const uint8_t raw[] = {0xAA, 0xBB, 0xCC, 0xDD};
    EXPECT_EQ(opensomeip_serializer_write_bytes(ser, raw, 4), OPENSOMEIP_RESULT_SUCCESS);

    uint8_t buf[16];
    size_t buf_len = sizeof(buf);
    opensomeip_serializer_get_data(ser, buf, &buf_len);

    opensomeip_deserializer_t* de = nullptr;
    ASSERT_EQ(opensomeip_deserializer_create(&de, buf, buf_len), OPENSOMEIP_RESULT_SUCCESS);

    uint8_t out[8];
    size_t out_len = 4;
    EXPECT_EQ(opensomeip_deserializer_read_bytes(de, out, &out_len), OPENSOMEIP_RESULT_SUCCESS);
    EXPECT_EQ(out_len, 4u);
    EXPECT_EQ(std::memcmp(out, raw, 4), 0);

    opensomeip_deserializer_destroy(de);
    opensomeip_serializer_destroy(ser);
}

/**
 * @test_case TC_CAPI_SER_RESET_001
 * @tests REQ_CAPI_008
 */
TEST(CapiSerializer, ResetClearsBuffer) {
    opensomeip_serializer_t* ser = nullptr;
    ASSERT_EQ(opensomeip_serializer_create(&ser), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_serializer_write_uint32(ser, 0x12345678);
    size_t sz = 0;
    opensomeip_serializer_get_size(ser, &sz);
    EXPECT_EQ(sz, 4u);

    EXPECT_EQ(opensomeip_serializer_reset(ser), OPENSOMEIP_RESULT_SUCCESS);
    opensomeip_serializer_get_size(ser, &sz);
    EXPECT_EQ(sz, 0u);

    opensomeip_serializer_destroy(ser);
}

/**
 * @test_case TC_CAPI_SER_OVERFLOW_001
 * @tests REQ_CAPI_002
 */
TEST(CapiSerializer, GetDataUndersizedBuffer) {
    opensomeip_serializer_t* ser = nullptr;
    ASSERT_EQ(opensomeip_serializer_create(&ser), OPENSOMEIP_RESULT_SUCCESS);

    opensomeip_serializer_write_uint32(ser, 0xFFFFFFFF);

    uint8_t buf[2];
    size_t buf_len = 2;
    EXPECT_EQ(opensomeip_serializer_get_data(ser, buf, &buf_len),
              OPENSOMEIP_RESULT_BUFFER_OVERFLOW);
    EXPECT_EQ(buf_len, 4u);

    opensomeip_serializer_destroy(ser);
}

/**
 * @test_case TC_CAPI_DESER_NULL_001
 * @tests REQ_CAPI_002
 */
TEST(CapiDeserializer, NullRejectsGracefully) {
    EXPECT_EQ(opensomeip_deserializer_create(nullptr, nullptr, 0),
              OPENSOMEIP_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(opensomeip_deserializer_destroy(nullptr),
              OPENSOMEIP_RESULT_INVALID_ARGUMENT);
}
