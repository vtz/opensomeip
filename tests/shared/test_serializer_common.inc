/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared serializer tests for RTOS and Zephyr test suites.
 *
 * Covers uint8/16/32/64 roundtrips, string serialization,
 * and buffer boundary checks — the platform-independent
 * subset of the host test_serialization.cpp suite.
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 */

#ifndef SOMEIP_TEST_SERIALIZER_COMMON_INC
#define SOMEIP_TEST_SERIALIZER_COMMON_INC

#include "serialization/serializer.h"
#include "platform/buffer_pool.h"
#include <cstdio>
#include <cstring>

static void test_serializer() {
    using namespace someip::serialization;

    printf("\n--- Serializer tests ---\n");

    // Basic scalar roundtrip
    {
        Serializer ser;
        ser.serialize_uint8(0x42);
        ser.serialize_uint16(0x1234);
        ser.serialize_uint32(0xDEADBEEF);

        auto data = ser.get_buffer();
        CHECK(data.size() == 7, "serialized_size");

        Deserializer deser(data);
        auto v8 = deser.deserialize_uint8();
        auto v16 = deser.deserialize_uint16();
        auto v32 = deser.deserialize_uint32();
        CHECK(v8.is_success() && v8.get_value() == 0x42, "deser_uint8");
        CHECK(v16.is_success() && v16.get_value() == 0x1234, "deser_uint16");
        CHECK(v32.is_success() && v32.get_value() == 0xDEADBEEF, "deser_uint32");
    }

    // uint64 roundtrip
    {
        Serializer ser;
        ser.serialize_uint64(0xCAFEBABEDEADFACEULL);
        auto data = ser.get_buffer();
        CHECK(data.size() == 8, "uint64_serialized_size");

        Deserializer deser(data);
        auto v64 = deser.deserialize_uint64();
        CHECK(v64.is_success() && v64.get_value() == 0xCAFEBABEDEADFACEULL,
              "deser_uint64");
    }

    // Boundary values
    {
        Serializer ser;
        ser.serialize_uint8(0x00);
        ser.serialize_uint8(0xFF);
        ser.serialize_uint16(0x0000);
        ser.serialize_uint16(0xFFFF);
        ser.serialize_uint32(0x00000000);
        ser.serialize_uint32(0xFFFFFFFF);

        auto data = ser.get_buffer();
        Deserializer deser(data);

        auto min8 = deser.deserialize_uint8();
        auto max8 = deser.deserialize_uint8();
        auto min16 = deser.deserialize_uint16();
        auto max16 = deser.deserialize_uint16();
        auto min32 = deser.deserialize_uint32();
        auto max32 = deser.deserialize_uint32();

        CHECK(min8.is_success() && min8.get_value() == 0x00, "boundary_min_u8");
        CHECK(max8.is_success() && max8.get_value() == 0xFF, "boundary_max_u8");
        CHECK(min16.is_success() && min16.get_value() == 0x0000, "boundary_min_u16");
        CHECK(max16.is_success() && max16.get_value() == 0xFFFF, "boundary_max_u16");
        CHECK(min32.is_success() && min32.get_value() == 0x00000000, "boundary_min_u32");
        CHECK(max32.is_success() && max32.get_value() == 0xFFFFFFFF, "boundary_max_u32");
    }

    // Underflow: reading from empty buffer
    {
        someip::platform::ByteBuffer empty;
        Deserializer deser(empty);
        auto v = deser.deserialize_uint8();
        CHECK(!v.is_success(), "empty_buffer_fails");
    }

    // String roundtrip
    {
        Serializer ser;
        const char* input = "SOME/IP";
        someip::platform::String<> input_str(input);
        ser.serialize_string(input_str);
        auto data = ser.get_buffer();

        Deserializer deser(data);
        auto result = deser.deserialize_string();
        CHECK(result.is_success() &&
              result.get_value().size() == 7 &&
              std::memcmp(result.get_value().data(), input, 7) == 0,
              "string_roundtrip");
    }
}

#endif // SOMEIP_TEST_SERIALIZER_COMMON_INC
