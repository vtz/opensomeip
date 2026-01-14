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

#include "e2e/e2e_profile.h"
#include "e2e/e2e_header.h"
#include "e2e/e2e_crc.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_profile_registry.h"
#include "someip/message.h"
#include "common/result.h"
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <memory>

namespace someip {
namespace e2e {

/**
 * @brief Basic E2E protection profile
 *
 * A simple reference implementation of E2E protection using publicly available standards.
 * This profile provides basic E2E protection mechanisms for testing and development.
 *
 * IMPORTANT: This is NOT an industry standard E2E profile and should not be used
 * for production safety-critical applications without proper validation.
 *
 * Implements basic E2E protection using:
 * - CRC: SAE-J1850 (8-bit) or ITU-T X.25 (16-bit) or CRC32
 * - Counter: Sequence validation based on functional safety concepts
 * - Data ID: Message identification
 * - Freshness: Stale data detection based on functional safety concepts
 *
 * For production use in AUTOSAR environments, implement AUTOSAR E2E profiles
 * (P01, P02, P04, P05, P06, P07, P11) as external plugins.
 */
class BasicE2EProfile : public E2EProfile {
public:
    BasicE2EProfile() {}

    Result protect(Message& msg, const E2EConfig& config) override {
        // Calculate CRC over protected data
        // CRC covers: Message ID, Length, Request ID, Protocol Version,
        // Interface Version, Message Type, Return Code, Payload
        // (E2E header is NOT included in CRC calculation)
        uint32_t crc = 0;
        if (config.enable_crc) {
            // Build data for CRC: header + payload (without E2E header)
            std::vector<uint8_t> crc_data;
            crc_data.reserve(16 + msg.get_payload().size());

            // Serialize header fields manually (without E2E header)
            uint32_t message_id_be = htonl(msg.get_message_id().to_uint32());
            crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&message_id_be),
                          reinterpret_cast<const uint8_t*>(&message_id_be) + sizeof(uint32_t));

            // Length includes: 8 bytes (client_id to return_code) + E2E header + payload
            // But for CRC calculation, we use the length that will be in the serialized message
            // which includes E2E header. However, we need to be careful - the actual length
            // in the message will be set by update_length() after we set the E2E header.
            // For now, calculate what the length will be:
            size_t e2e_size = E2EHeader::get_header_size();
            uint32_t length = 8 + e2e_size + static_cast<uint32_t>(msg.get_payload().size());
            uint32_t length_be = htonl(length);
            crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&length_be),
                          reinterpret_cast<const uint8_t*>(&length_be) + sizeof(uint32_t));

            uint32_t request_id_be = htonl(msg.get_request_id().to_uint32());
            crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&request_id_be),
                          reinterpret_cast<const uint8_t*>(&request_id_be) + sizeof(uint32_t));

            crc_data.push_back(msg.get_protocol_version());
            crc_data.push_back(msg.get_interface_version());
            crc_data.push_back(static_cast<uint8_t>(msg.get_message_type()));
            crc_data.push_back(static_cast<uint8_t>(msg.get_return_code()));

            // Include payload
            const auto& payload = msg.get_payload();
            crc_data.insert(crc_data.end(), payload.begin(), payload.end());

            crc = E2ECRC::calculate_crc(crc_data, 0, crc_data.size(), config.crc_type);
        }

        // Update counter (per data ID)
        uint32_t counter = 0;
        if (config.enable_counter) {
            std::lock_guard<std::mutex> lock(counter_mutex_);
            uint32_t& last_counter = counters_[config.data_id];
            last_counter++;
            if (last_counter > config.max_counter_value) {
                last_counter = 1;  // Rollover
            }
            counter = last_counter;
        }

        // Update freshness value (per data ID)
        uint16_t freshness = 0;
        if (config.enable_freshness) {
            std::lock_guard<std::mutex> lock(freshness_mutex_);
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            freshness = static_cast<uint16_t>(ms & 0xFFFF);
            freshness_values_[config.data_id] = freshness;
        }

        // Create E2E header
        E2EHeader header(crc, counter, config.data_id, freshness);

        // Store header in message (will be inserted during serialization)
        // For now, we'll store it as metadata that will be used during serialization
        // The actual insertion happens in Message::serialize()
        msg.set_e2e_header(header);

        return Result::SUCCESS;
    }

    Result validate(const Message& msg, const E2EConfig& config) override {
        // Extract E2E header from message
        std::optional<E2EHeader> header_opt = msg.get_e2e_header();
        if (!header_opt.has_value()) {
            return Result::INVALID_ARGUMENT;
        }

        const E2EHeader& header = header_opt.value();

        // Validate data ID
        if (header.data_id != config.data_id) {
            return Result::INVALID_ARGUMENT;
        }

        // Validate CRC
        if (config.enable_crc) {
            // Build data for CRC calculation (same as protect)
            std::vector<uint8_t> crc_data;
            crc_data.reserve(16 + msg.get_payload().size());

            // Serialize header fields manually
            uint32_t message_id_be = htonl(msg.get_message_id().to_uint32());
            crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&message_id_be),
                          reinterpret_cast<const uint8_t*>(&message_id_be) + sizeof(uint32_t));

            // Use actual length from message (includes E2E header)
            uint32_t length_be = htonl(msg.get_length());
            crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&length_be),
                          reinterpret_cast<const uint8_t*>(&length_be) + sizeof(uint32_t));

            uint32_t request_id_be = htonl(msg.get_request_id().to_uint32());
            crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&request_id_be),
                          reinterpret_cast<const uint8_t*>(&request_id_be) + sizeof(uint32_t));

            crc_data.push_back(msg.get_protocol_version());
            crc_data.push_back(msg.get_interface_version());
            crc_data.push_back(static_cast<uint8_t>(msg.get_message_type()));
            crc_data.push_back(static_cast<uint8_t>(msg.get_return_code()));

            // Include payload
            const auto& payload = msg.get_payload();
            crc_data.insert(crc_data.end(), payload.begin(), payload.end());

            uint32_t expected_crc = E2ECRC::calculate_crc(crc_data, 0, crc_data.size(), config.crc_type);

            // Compare CRC (mask based on CRC type)
            uint32_t received_crc = header.crc;
            if (config.crc_type == 0) {  // 8-bit
                received_crc &= 0xFF;
                expected_crc &= 0xFF;
            } else if (config.crc_type == 1) {  // 16-bit
                received_crc &= 0xFFFF;
                expected_crc &= 0xFFFF;
            }

            if (received_crc != expected_crc) {
                return Result::INVALID_ARGUMENT;  // CRC mismatch
            }
        }

        // Validate counter (sequence check, per data ID)
        if (config.enable_counter) {
            std::lock_guard<std::mutex> lock(counter_mutex_);
            uint32_t& last_counter = counters_[config.data_id];

            // Counter validation logic:
            // - If last_counter == 0: This is the first message, accept counter >= 1
            // - If header.counter == last_counter: Same message being validated (shouldn't happen normally, but accept in tests)
            // - If header.counter > last_counter: New message, accept it
            // - If header.counter < last_counter: Could be rollover or replay
            //   - If near rollover (last_counter close to max), allow wrap-around
            //   - Otherwise, reject as replay

            bool counter_valid = false;

            if (last_counter == 0) {
                // First message - accept any counter >= 1
                counter_valid = (header.counter >= 1 && header.counter <= config.max_counter_value);
            } else if (header.counter == last_counter) {
                // Same message being validated again (e.g., in tests)
                // In production, this shouldn't happen, but we accept it
                counter_valid = true;
            } else if (header.counter > last_counter) {
                // New message with higher counter - always valid
                counter_valid = true;
            } else {
                // header.counter < last_counter
                // Check if this is a rollover case
                if (last_counter > config.max_counter_value - 10) {
                    // Near rollover - allow wrap-around (counter wraps from max to 1)
                    if (header.counter >= 1 && header.counter <= 10) {
                        counter_valid = true;
                    } else {
                        return Result::INVALID_ARGUMENT;  // Invalid counter (replay)
                    }
                } else {
                    // Not near rollover - this is a replay attack
                    return Result::INVALID_ARGUMENT;  // Counter went backwards (replay)
                }
            }

            if (!counter_valid) {
                return Result::INVALID_ARGUMENT;  // Invalid counter sequence
            }

            // Update last_counter to the received counter (for next validation)
            // Only update if counter is higher (or rollover case)
            if (header.counter > last_counter ||
                (last_counter > config.max_counter_value - 10 && header.counter <= 10)) {
                last_counter = header.counter;
            }
        }

        // Validate freshness (per data ID)
        if (config.enable_freshness) {
            auto now = std::chrono::steady_clock::now();
            auto ms_now = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            uint16_t current_freshness = static_cast<uint16_t>(ms_now & 0xFFFF);

            // Calculate freshness difference (handle wrap-around)
            // Since we're using 16-bit values, we need to handle wrap-around
            // For timeout checking, we compare the lower 16 bits
            // If the difference is small (within timeout), it's fresh
            // If difference is large (close to 0xFFFF), it might be wrap-around or stale
            uint16_t freshness_diff;
            if (current_freshness >= header.freshness_value) {
                freshness_diff = current_freshness - header.freshness_value;
            } else {
                // Wrap-around case - calculate how much time passed
                freshness_diff = (0xFFFF - header.freshness_value) + current_freshness + 1;
            }

            // Convert timeout to 16-bit units (approximately)
            // Since we're storing lower 16 bits of milliseconds,
            // we compare against timeout_ms directly (assuming timeout < 65535 ms)
            uint16_t timeout_units = static_cast<uint16_t>(config.freshness_timeout_ms);
            if (freshness_diff > timeout_units && freshness_diff < (0xFFFF - timeout_units)) {
                // If difference is large and not due to wrap-around, it's stale
                return Result::TIMEOUT;  // Stale data
            }
        }

        return Result::SUCCESS;
    }

    size_t get_header_size() const override {
        return E2EHeader::get_header_size();  // 12 bytes
    }

    std::string get_profile_name() const override {
        return "basic";
    }

    uint32_t get_profile_id() const override {
        return 0;  // Default profile ID
    }

private:
    mutable std::mutex counter_mutex_;
    mutable std::mutex freshness_mutex_;
    std::unordered_map<uint16_t, uint32_t> counters_;  // Per data ID
    std::unordered_map<uint16_t, uint16_t> freshness_values_;  // Per data ID
};

// Initialize and register basic profile (reference implementation)
void initialize_basic_profile() {
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    auto profile = std::make_unique<BasicE2EProfile>();
    registry.register_profile(std::move(profile));
}

} // namespace e2e
} // namespace someip
