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

#include "serialization/serializer.h"
#include <cstring>
#include <algorithm>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

namespace someip {
namespace serialization {

/**
 * @brief SOME/IP Serializer implementation
 * @implements REQ_ARCH_001
 * @satisfies feat_req_someip_600
 * @satisfies feat_req_someip_601
 * @satisfies feat_req_someip_602
 */

// NOLINTNEXTLINE(modernize-use-equals-default,readability-redundant-member-init) - intentional pre-allocation
Serializer::Serializer() {
    buffer_.reserve(1024);  // Pre-allocate reasonable size
}

void Serializer::reset() {
    buffer_.clear();
}

void Serializer::serialize_bool(bool value) {
    buffer_.push_back(value ? 0x01 : 0x00);
}

void Serializer::serialize_uint8(uint8_t value) {
    buffer_.push_back(value);
}

void Serializer::serialize_uint16(uint16_t value) {
    append_be_uint16(value);
}

void Serializer::serialize_uint32(uint32_t value) {
    append_be_uint32(value);
}

void Serializer::serialize_uint64(uint64_t value) {
    append_be_uint64(value);
}

void Serializer::serialize_int8(int8_t value) {
    buffer_.push_back(static_cast<uint8_t>(value));
}

void Serializer::serialize_int16(int16_t value) {
    append_be_int16(value);
}

void Serializer::serialize_int32(int32_t value) {
    append_be_int32(value);
}

void Serializer::serialize_int64(int64_t value) {
    append_be_int64(value);
}

void Serializer::serialize_float(float value) {
    append_be_float(value);
}

void Serializer::serialize_double(double value) {
    append_be_double(value);
}

void Serializer::serialize_string(const std::string& value) {
    // Serialize string length as uint32_t
    serialize_uint32(static_cast<uint32_t>(value.length()));

    // Serialize string data (no null terminator)
    buffer_.insert(buffer_.end(), value.begin(), value.end());

    // Add padding to align to 4-byte boundary
    align_to(4);
}

void Serializer::align_to(size_t alignment) {
    size_t current_size = buffer_.size();
    size_t padding_needed = (alignment - (current_size % alignment)) % alignment;

    for (size_t i = 0; i < padding_needed; ++i) {
        buffer_.push_back(0x00);
    }
}

void Serializer::add_padding(size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) {
        buffer_.push_back(0x00);
    }
}

void Serializer::append_be_uint16(uint16_t value) {
    uint16_t be_value = htons(value);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&be_value);
    buffer_.insert(buffer_.end(), bytes, bytes + sizeof(uint16_t));
}

void Serializer::append_be_uint32(uint32_t value) {
    uint32_t be_value = htonl(value);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&be_value);
    buffer_.insert(buffer_.end(), bytes, bytes + sizeof(uint32_t));
}

void Serializer::append_be_uint64(uint64_t value) {
    // Manual big-endian conversion for macOS compatibility
    uint64_t be_value = ((value & 0xFF00000000000000ULL) >> 56) |
                        ((value & 0x00FF000000000000ULL) >> 40) |
                        ((value & 0x0000FF0000000000ULL) >> 24) |
                        ((value & 0x000000FF00000000ULL) >> 8) |
                        ((value & 0x00000000FF000000ULL) << 8) |
                        ((value & 0x0000000000FF0000ULL) << 24) |
                        ((value & 0x000000000000FF00ULL) << 40) |
                        ((value & 0x00000000000000FFULL) << 56);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&be_value);
    buffer_.insert(buffer_.end(), bytes, bytes + sizeof(uint64_t));
}

void Serializer::append_be_int16(int16_t value) {
    append_be_uint16(static_cast<uint16_t>(value));
}

void Serializer::append_be_int32(int32_t value) {
    append_be_uint32(static_cast<uint32_t>(value));
}

void Serializer::append_be_int64(int64_t value) {
    append_be_uint64(static_cast<uint64_t>(value));
}

void Serializer::append_be_float(float value) {
    // Convert to big-endian bytes
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    append_be_uint32(bits);
}

void Serializer::append_be_double(double value) {
    // Convert to big-endian bytes using memcpy to avoid undefined behavior
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    append_be_uint64(bits);
}

// Deserializer implementation

Deserializer::Deserializer(const std::vector<uint8_t>& data)
    : buffer_(data), position_(0) {
}

Deserializer::Deserializer(std::vector<uint8_t>&& data)
    : buffer_(std::move(data)), position_(0) {
}

void Deserializer::reset() {
    position_ = 0;
}

DeserializationResult<bool> Deserializer::deserialize_bool() {
    if (position_ + sizeof(uint8_t) > buffer_.size()) {
        return DeserializationResult<bool>::error(Result::MALFORMED_MESSAGE);
    }
    bool value = buffer_[position_++] != 0x00;
    return DeserializationResult<bool>::success(value);
}

DeserializationResult<uint8_t> Deserializer::deserialize_uint8() {
    if (position_ + sizeof(uint8_t) > buffer_.size()) {
        return DeserializationResult<uint8_t>::error(Result::MALFORMED_MESSAGE);
    }
    uint8_t value = buffer_[position_++];
    return DeserializationResult<uint8_t>::success(value);
}

DeserializationResult<uint16_t> Deserializer::deserialize_uint16() {
    auto result = read_be_uint16();
    if (!result) {
        return DeserializationResult<uint16_t>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<uint16_t>::success(*result);
}

DeserializationResult<uint32_t> Deserializer::deserialize_uint32() {
    auto result = read_be_uint32();
    if (!result) {
        return DeserializationResult<uint32_t>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<uint32_t>::success(*result);
}

DeserializationResult<uint64_t> Deserializer::deserialize_uint64() {
    auto result = read_be_uint64();
    if (!result) {
        return DeserializationResult<uint64_t>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<uint64_t>::success(*result);
}

DeserializationResult<int8_t> Deserializer::deserialize_int8() {
    auto result = deserialize_uint8();
    if (result.is_error()) {
        return DeserializationResult<int8_t>::error(result.get_error());
    }
    return DeserializationResult<int8_t>::success(static_cast<int8_t>(result.get_value()));
}

DeserializationResult<int16_t> Deserializer::deserialize_int16() {
    auto result = read_be_uint16();
    if (!result) {
        return DeserializationResult<int16_t>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<int16_t>::success(static_cast<int16_t>(*result));
}

DeserializationResult<int32_t> Deserializer::deserialize_int32() {
    auto result = read_be_uint32();
    if (!result) {
        return DeserializationResult<int32_t>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<int32_t>::success(static_cast<int32_t>(*result));
}

DeserializationResult<int64_t> Deserializer::deserialize_int64() {
    auto result = read_be_uint64();
    if (!result) {
        return DeserializationResult<int64_t>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<int64_t>::success(static_cast<int64_t>(*result));
}

DeserializationResult<float> Deserializer::deserialize_float() {
    auto result = read_be_float();
    if (!result) {
        return DeserializationResult<float>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<float>::success(*result);
}

DeserializationResult<double> Deserializer::deserialize_double() {
    auto result = read_be_double();
    if (!result) {
        return DeserializationResult<double>::error(Result::MALFORMED_MESSAGE);
    }
    return DeserializationResult<double>::success(*result);
}

DeserializationResult<std::string> Deserializer::deserialize_string() {
    // Deserialize string length
    auto length_result = deserialize_uint32();
    if (length_result.is_error()) {
        return DeserializationResult<std::string>::error(length_result.get_error());
    }
    uint32_t length = length_result.get_value();

    if (position_ + length > buffer_.size()) {
        return DeserializationResult<std::string>::error(Result::MALFORMED_MESSAGE);
    }

    std::string result(buffer_.begin() + position_,
                      buffer_.begin() + position_ + length);
    position_ += length;

    // Skip padding to align to 4-byte boundary
    align_to(4);

    return DeserializationResult<std::string>::success(std::move(result));
}

bool Deserializer::set_position(size_t pos) {
    bool valid = pos <= buffer_.size();
    if (valid) {
        position_ = pos;
    }
    return valid;
}

void Deserializer::skip(size_t bytes) {
    position_ = std::min(position_ + bytes, buffer_.size());
}

void Deserializer::align_to(size_t alignment) {
    size_t padding = (alignment - (position_ % alignment)) % alignment;
    skip(padding);
}

std::optional<uint16_t> Deserializer::read_be_uint16() {
    if (position_ + sizeof(uint16_t) > buffer_.size()) {
        return std::nullopt;
    }

    uint16_t value;
    std::memcpy(&value, &buffer_[position_], sizeof(uint16_t));
    position_ += sizeof(uint16_t);
    return ntohs(value);
}

std::optional<uint32_t> Deserializer::read_be_uint32() {
    if (position_ + sizeof(uint32_t) > buffer_.size()) {
        return std::nullopt;
    }

    uint32_t value;
    std::memcpy(&value, &buffer_[position_], sizeof(uint32_t));
    position_ += sizeof(uint32_t);
    return ntohl(value);
}

std::optional<uint64_t> Deserializer::read_be_uint64() {
    if (position_ + sizeof(uint64_t) > buffer_.size()) {
        return std::nullopt;
    }

    uint64_t be_value;
    std::memcpy(&be_value, &buffer_[position_], sizeof(uint64_t));
    position_ += sizeof(uint64_t);

    // Manual big-endian to host conversion for macOS compatibility
    return ((be_value & 0xFF00000000000000ULL) >> 56) |
           ((be_value & 0x00FF000000000000ULL) >> 40) |
           ((be_value & 0x0000FF0000000000ULL) >> 24) |
           ((be_value & 0x000000FF00000000ULL) >> 8) |
           ((be_value & 0x00000000FF000000ULL) << 8) |
           ((be_value & 0x0000000000FF0000ULL) << 24) |
           ((be_value & 0x000000000000FF00ULL) << 40) |
           ((be_value & 0x00000000000000FFULL) << 56);
}

std::optional<int16_t> Deserializer::read_be_int16() {
    auto result = read_be_uint16();
    if (!result) {
        return std::nullopt;
    }
    return static_cast<int16_t>(*result);
}

std::optional<int32_t> Deserializer::read_be_int32() {
    auto result = read_be_uint32();
    if (!result) {
        return std::nullopt;
    }
    return static_cast<int32_t>(*result);
}

std::optional<int64_t> Deserializer::read_be_int64() {
    auto result = read_be_uint64();
    if (!result) {
        return std::nullopt;
    }
    return static_cast<int64_t>(*result);
}

std::optional<float> Deserializer::read_be_float() {
    auto bits = read_be_uint32();
    if (!bits) {
        return std::nullopt;
    }
    float result;
    std::memcpy(&result, &*bits, sizeof(result));
    return result;
}

std::optional<double> Deserializer::read_be_double() {
    auto bits = read_be_uint64();
    if (!bits) {
        return std::nullopt;
    }
    double result;
    std::memcpy(&result, &*bits, sizeof(result));
    return result;
}

} // namespace serialization
} // namespace someip
