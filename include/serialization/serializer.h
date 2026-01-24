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

#ifndef SOMEIP_SERIALIZATION_SERIALIZER_H
#define SOMEIP_SERIALIZATION_SERIALIZER_H

#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <optional>
#include "../common/result.h"

namespace someip {
namespace serialization {

/**
 * @brief Result of a deserialization operation
 *
 * This template represents the result of deserializing a value.
 * It either contains the successfully deserialized value, or indicates
 * an error condition (e.g., insufficient data).
 */
template<typename T>
class DeserializationResult {
public:
    /**
     * @brief Create a successful result with a value
     */
    static DeserializationResult success(T value) {
        DeserializationResult result;
        result.value_ = std::move(value);
        result.error_ = Result::SUCCESS;
        return result;
    }

    /**
     * @brief Create an error result
     */
    static DeserializationResult error(Result error_code) {
        DeserializationResult result;
        result.error_ = error_code;
        return result;
    }

    /**
     * @brief Check if the operation was successful
     */
    bool is_success() const {
        return error_ == Result::SUCCESS;
    }

    /**
     * @brief Check if the operation failed
     */
    bool is_error() const {
        return error_ != Result::SUCCESS;
    }

    /**
     * @brief Get the error code (only valid if is_error() returns true)
     */
    Result get_error() const {
        return error_;
    }

    /**
     * @brief Get the value (only valid if is_success() returns true)
     */
    const T& get_value() const {
        return value_.value();
    }

    /**
     * @brief Get the value with move semantics (only valid if is_success() returns true)
     */
    T&& move_value() {
        return std::move(value_.value());
    }

private:
    std::optional<T> value_;
    Result error_;
};

/**
 * @brief SOME/IP data serializer
 *
 * This class provides serialization capabilities for SOME/IP data types
 * following the SOME/IP serialization rules (big-endian, aligned).
 */
class Serializer {
public:
    /**
     * @brief Constructor
     */
    Serializer();

    /**
     * @brief Destructor
     */
    ~Serializer() = default;

    /**
     * @brief Reset the serializer (clear buffer)
     */
    void reset();

    // Basic type serialization
    void serialize_bool(bool value);
    void serialize_uint8(uint8_t value);
    void serialize_uint16(uint16_t value);
    void serialize_uint32(uint32_t value);
    void serialize_uint64(uint64_t value);
    void serialize_int8(int8_t value);
    void serialize_int16(int16_t value);
    void serialize_int32(int32_t value);
    void serialize_int64(int64_t value);
    void serialize_float(float value);
    void serialize_double(double value);
    void serialize_string(const std::string& value);

    // Array serialization
    template<typename T>
    void serialize_array(const std::vector<T>& array);

    // Get serialized data
    const std::vector<uint8_t>& get_buffer() const { return buffer_; }
    std::vector<uint8_t>&& move_buffer() { return std::move(buffer_); }
    size_t get_size() const { return buffer_.size(); }

    // Utility methods
    void align_to(size_t alignment);
    void add_padding(size_t bytes);

private:
    std::vector<uint8_t> buffer_;

    // Helper methods for endianness conversion
    void append_be_uint16(uint16_t value);
    void append_be_uint32(uint32_t value);
    void append_be_uint64(uint64_t value);
    void append_be_int16(int16_t value);
    void append_be_int32(int32_t value);
    void append_be_int64(int64_t value);
    void append_be_float(float value);
    void append_be_double(double value);
};

/**
 * @brief SOME/IP data deserializer
 *
 * This class provides deserialization capabilities for SOME/IP data types
 * following the SOME/IP serialization rules.
 */
class Deserializer {
public:
    /**
     * @brief Constructor
     * @param data The data to deserialize from
     */
    explicit Deserializer(const std::vector<uint8_t>& data);

    /**
     * @brief Constructor with data ownership
     * @param data The data to deserialize from (moved)
     */
    explicit Deserializer(std::vector<uint8_t>&& data);

    /**
     * @brief Destructor
     */
    ~Deserializer() = default;

    /**
     * @brief Reset position to beginning
     */
    void reset();

    // Basic type deserialization
    DeserializationResult<bool> deserialize_bool();
    DeserializationResult<uint8_t> deserialize_uint8();
    DeserializationResult<uint16_t> deserialize_uint16();
    DeserializationResult<uint32_t> deserialize_uint32();
    DeserializationResult<uint64_t> deserialize_uint64();
    DeserializationResult<int8_t> deserialize_int8();
    DeserializationResult<int16_t> deserialize_int16();
    DeserializationResult<int32_t> deserialize_int32();
    DeserializationResult<int64_t> deserialize_int64();
    DeserializationResult<float> deserialize_float();
    DeserializationResult<double> deserialize_double();
    DeserializationResult<std::string> deserialize_string();

    // Array deserialization
    template<typename T>
    DeserializationResult<std::vector<T>> deserialize_array(size_t length);

    // Dynamic array deserialization with length validation
    template<typename T>
    DeserializationResult<std::vector<T>> deserialize_dynamic_array();

    // Status and navigation
    bool is_valid() const { return position_ <= buffer_.size(); }
    size_t get_position() const { return position_; }
    size_t get_remaining() const { return buffer_.size() - position_; }
    bool set_position(size_t pos);
    void skip(size_t bytes);

    // Utility methods
    void align_to(size_t alignment);

private:
    std::vector<uint8_t> buffer_;
    size_t position_;

    // Helper methods for endianness conversion
    std::optional<uint16_t> read_be_uint16();
    std::optional<uint32_t> read_be_uint32();
    std::optional<uint64_t> read_be_uint64();
    std::optional<int16_t> read_be_int16();
    std::optional<int32_t> read_be_int32();
    std::optional<int64_t> read_be_int64();
    std::optional<float> read_be_float();
    std::optional<double> read_be_double();
};

// Template implementations (must be in header)

template<typename T>
void Serializer::serialize_array(const std::vector<T>& array) {
    // Serialize array length as uint32_t
    serialize_uint32(static_cast<uint32_t>(array.size()));

    // Serialize each element
    for (const auto& element : array) {
        if constexpr (std::is_same_v<T, bool>) {
            serialize_bool(element);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            serialize_uint8(element);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            serialize_uint16(element);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            serialize_uint32(element);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            serialize_uint64(element);
        } else if constexpr (std::is_same_v<T, int8_t>) {
            serialize_int8(element);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            serialize_int16(element);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            serialize_int32(element);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            serialize_int64(element);
        } else if constexpr (std::is_same_v<T, float>) {
            serialize_float(element);
        } else if constexpr (std::is_same_v<T, double>) {
            serialize_double(element);
        } else if constexpr (std::is_same_v<T, std::string>) {
            serialize_string(element);
        } else {
            // For complex types, static_assert will catch this at compile time
            static_assert(sizeof(T) == 0, "Unsupported array element type for serialization");
        }
    }
}

template<typename T>
DeserializationResult<std::vector<T>> Deserializer::deserialize_array(size_t length) {
    std::vector<T> result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        DeserializationResult<T> element_result;

        if constexpr (std::is_same_v<T, bool>) {
            element_result = deserialize_bool();
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            element_result = deserialize_uint8();
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            element_result = deserialize_uint16();
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            element_result = deserialize_uint32();
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            element_result = deserialize_uint64();
        } else if constexpr (std::is_same_v<T, int8_t>) {
            element_result = deserialize_int8();
        } else if constexpr (std::is_same_v<T, int16_t>) {
            element_result = deserialize_int16();
        } else if constexpr (std::is_same_v<T, int32_t>) {
            element_result = deserialize_int32();
        } else if constexpr (std::is_same_v<T, int64_t>) {
            element_result = deserialize_int64();
        } else if constexpr (std::is_same_v<T, float>) {
            element_result = deserialize_float();
        } else if constexpr (std::is_same_v<T, double>) {
            element_result = deserialize_double();
        } else if constexpr (std::is_same_v<T, std::string>) {
            element_result = deserialize_string();
        } else {
            // For complex types, static_assert will catch this at compile time
            static_assert(sizeof(T) == 0, "Unsupported array element type for deserialization");
        }

        if (element_result.is_error()) {
            return DeserializationResult<std::vector<T>>::error(element_result.get_error());
        }

        result.push_back(element_result.move_value());
    }

    return DeserializationResult<std::vector<T>>::success(std::move(result));
}

template<typename T>
DeserializationResult<std::vector<T>> Deserializer::deserialize_dynamic_array() {
    // Read length prefix (in bytes)
    auto length_result = deserialize_uint32();
    if (length_result.is_error()) {
        return DeserializationResult<std::vector<T>>::error(length_result.get_error());
    }

    uint32_t byte_length = length_result.get_value();

    // Validate that byte length is multiple of element size (REQ_SER_082_E01)
    size_t element_size = sizeof(T);
    if (byte_length % element_size != 0) {
        return DeserializationResult<std::vector<T>>::error(Result::MALFORMED_MESSAGE);
    }

    size_t element_count = byte_length / element_size;

    // Deserialize array elements
    return deserialize_array<T>(element_count);
}

} // namespace serialization
} // namespace someip

#endif // SOMEIP_SERIALIZATION_SERIALIZER_H
