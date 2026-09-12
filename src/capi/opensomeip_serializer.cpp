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
 * @file opensomeip_serializer.cpp
 * @brief C API wrappers for serializer/deserializer operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_007, REQ_CAPI_008
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "serialization/serializer.h"
#include <cstring>

struct opensomeip_serializer_s {
    someip::serialization::Serializer ser;
};

struct opensomeip_deserializer_s {
    someip::serialization::Deserializer de;
    explicit opensomeip_deserializer_s(someip::platform::ByteBuffer&& buf)
        : de(std::move(buf)) {}
};

extern "C" opensomeip_result_t opensomeip_serializer_create(opensomeip_serializer_t** out) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_serializer_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_destroy(opensomeip_serializer_t* ser) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete ser; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_reset(opensomeip_serializer_t* ser) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { ser->ser.reset(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_write_uint8(opensomeip_serializer_t* ser, uint8_t val) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { ser->ser.serialize_uint8(val); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_write_uint16(opensomeip_serializer_t* ser, uint16_t val) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { ser->ser.serialize_uint16(val); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_write_uint32(opensomeip_serializer_t* ser, uint32_t val) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { ser->ser.serialize_uint32(val); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_write_uint64(opensomeip_serializer_t* ser, uint64_t val) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { ser->ser.serialize_uint64(val); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_write_bytes(opensomeip_serializer_t* ser,
                                                                  const uint8_t* data, size_t len) {
    if (!ser) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (len > 0 && !data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        for (size_t i = 0; i < len; ++i) {
            ser->ser.serialize_uint8(data[i]);
        }
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_get_data(const opensomeip_serializer_t* ser,
                                                               uint8_t* buf, size_t* out_len) {
    if (!ser || !out_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        const auto& data = ser->ser.get_buffer();
        if (*out_len < data.size()) {
            *out_len = data.size();
            return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
        }
        if (!data.empty()) {
            if (!buf) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
            std::memcpy(buf, data.data(), data.size());
        }
        *out_len = data.size();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_serializer_get_size(const opensomeip_serializer_t* ser,
                                                               size_t* out_len) {
    if (!ser || !out_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out_len = ser->ser.get_size(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_create(opensomeip_deserializer_t** out,
                                                               const uint8_t* data, size_t len) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (len > 0 && !data) { *out = nullptr; return OPENSOMEIP_RESULT_INVALID_ARGUMENT; }
    CAPI_ENSURE_INIT();
    try {
        someip::platform::ByteBuffer buf(data, data + len);
        *out = new opensomeip_deserializer_s(std::move(buf));
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_destroy(opensomeip_deserializer_t* de) {
    if (!de) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete de; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_read_uint8(opensomeip_deserializer_t* de, uint8_t* out) {
    if (!de || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = de->de.deserialize_uint8();
        if (r.is_error()) return static_cast<opensomeip_result_t>(r.get_error());
        *out = r.get_value();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_read_uint16(opensomeip_deserializer_t* de, uint16_t* out) {
    if (!de || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = de->de.deserialize_uint16();
        if (r.is_error()) return static_cast<opensomeip_result_t>(r.get_error());
        *out = r.get_value();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_read_uint32(opensomeip_deserializer_t* de, uint32_t* out) {
    if (!de || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = de->de.deserialize_uint32();
        if (r.is_error()) return static_cast<opensomeip_result_t>(r.get_error());
        *out = r.get_value();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_read_uint64(opensomeip_deserializer_t* de, uint64_t* out) {
    if (!de || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        auto r = de->de.deserialize_uint64();
        if (r.is_error()) return static_cast<opensomeip_result_t>(r.get_error());
        *out = r.get_value();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_read_bytes(opensomeip_deserializer_t* de,
                                                                   uint8_t* buf, size_t* len) {
    if (!de || !len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    size_t requested = *len;
    if (requested > 0 && !buf) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        size_t remaining = de->de.get_remaining();
        size_t to_read = (requested < remaining) ? requested : remaining;
        for (size_t i = 0; i < to_read; ++i) {
            auto r = de->de.deserialize_uint8();
            if (r.is_error()) { *len = i; return static_cast<opensomeip_result_t>(r.get_error()); }
            buf[i] = r.get_value();
        }
        *len = to_read;
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_deserializer_get_remaining(const opensomeip_deserializer_t* de,
                                                                      size_t* out) {
    if (!de || !out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { *out = de->de.get_remaining(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
