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
#include "e2e/e2e_layout.h"
#include "e2e/e2e_profile_registry.h"
#include "someip/message.h"
#include "common/result.h"
#include "platform/thread.h"
// NOLINTNEXTLINE(misc-include-cleaner) - someip_htonl macro from byteorder_impl.h
#include "platform/byteorder.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace someip::e2e {
// NOLINTBEGIN(misc-include-cleaner) - someip_htonl macro from platform/byteorder.h -> byteorder_impl.h

namespace {

/**
 * CRC input for the basic profile: SOME/IP header fields and application
 * payload. The unprotected prefix and the E2E header itself are omitted.
 * length_field is the on-wire Length, which does include both.
 */
platform::ByteBuffer build_basic_crc_input(const Message& msg, uint32_t length_field) {
    platform::ByteBuffer crc_data;
    crc_data.reserve(16 + msg.get_payload().size());

    const uint32_t message_id_be = someip_htonl(msg.get_message_id().to_uint32());
    crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&message_id_be),
                    reinterpret_cast<const uint8_t*>(&message_id_be) + sizeof(uint32_t));

    const uint32_t length_be = someip_htonl(length_field);
    crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&length_be),
                    reinterpret_cast<const uint8_t*>(&length_be) + sizeof(uint32_t));

    const uint32_t request_id_be = someip_htonl(msg.get_request_id().to_uint32());
    crc_data.insert(crc_data.end(), reinterpret_cast<const uint8_t*>(&request_id_be),
                    reinterpret_cast<const uint8_t*>(&request_id_be) + sizeof(uint32_t));

    crc_data.push_back(msg.get_protocol_version());
    crc_data.push_back(msg.get_interface_version());
    crc_data.push_back(static_cast<uint8_t>(msg.get_message_type()));
    crc_data.push_back(static_cast<uint8_t>(msg.get_return_code()));

    const auto& payload = msg.get_payload();
    crc_data.insert(crc_data.end(), payload.begin(), payload.end());
    return crc_data;
}

}  // namespace

/**
 * @brief Basic E2E protection profile
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
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
    BasicE2EProfile() = default;

    /** @implements REQ_E2E_PLUGIN_001, REQ_E2E_PLUGIN_004 */
    Result protect(Message& msg, const E2EConfig& config) override {
        size_t expected_prefix = 0;
        Result const layout =
            check_e2e_layout(config.offset_bits, get_header_size(), expected_prefix);
        if (layout != Result::SUCCESS) {
            return layout;
        }
        if (msg.e2e_unprotected_prefix_size() != expected_prefix ||
            (expected_prefix > 0 && msg.e2e_offset_bits() != config.offset_bits)) {
            return Result::INVALID_ARGUMENT;
        }

        // CRC covers Message ID, Length, Request ID, Protocol Version,
        // Interface Version, Message Type, Return Code, and application payload.
        // The unprotected prefix and the E2E header are not covered. Length
        // still counts the prefix and the header (feat_req_someip_77).
        uint32_t crc = 0;
        const auto wire_length = static_cast<uint32_t>(
            8U + expected_prefix + get_header_size() + msg.get_payload().size());
        if (config.enable_crc) {
            const platform::ByteBuffer crc_data = build_basic_crc_input(msg, wire_length);

            auto crc_result = e2ecrc::calculate_crc(crc_data, 0, crc_data.size(), config.crc_type);
            if (!crc_result.has_value()) {
                return Result::INVALID_ARGUMENT;
            }
            crc = crc_result.value();
        }

        // Update counter (per data ID)
        uint32_t counter = 0;
        if (config.enable_counter) {
            platform::ScopedLock const lock(counter_mutex_);
            auto it = counters_.find(config.data_id);
            if (it == counters_.end()) {
                auto [ins_it, inserted] = counters_.insert({config.data_id, 0});
                if (!inserted) {
                    return Result::RESOURCE_EXHAUSTED;
                }
                it = ins_it;
            }
            uint32_t& last_counter = it->second;
            last_counter++;
            if (last_counter > config.max_counter_value) {
                last_counter = 1;  // Rollover
            }
            counter = last_counter;
        }

        // Update freshness value (per data ID)
        uint16_t freshness = 0;
        if (config.enable_freshness) {
            platform::ScopedLock const lock(freshness_mutex_);
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                          .count();
            freshness = static_cast<uint16_t>(static_cast<uint64_t>(ms) & 0xFFFFULL);
            auto it = freshness_values_.find(config.data_id);
            if (it == freshness_values_.end()) {
                auto [ins_it, inserted] = freshness_values_.insert({config.data_id, freshness});
                if (!inserted) {
                    return Result::RESOURCE_EXHAUSTED;
                }
            }
            else {
                it->second = freshness;
            }
        }

        // Create E2E header and place it at config.offset_bits.
        E2EHeader const header(crc, counter, config.data_id, freshness);
        std::array<uint8_t, E2EHeader::get_header_size()> raw{};
        header.write_to(raw.data());
        return msg.set_e2e_profile_bytes(config.offset_bits, raw.data(), raw.size(),
                                         msg.e2e_unprotected_prefix(), expected_prefix);
    }

    /** @implements REQ_E2E_PLUGIN_001, REQ_E2E_PLUGIN_004 */
    Result validate(const Message& msg, const E2EConfig& config) override {
        size_t expected_prefix = 0;
        Result const layout =
            check_e2e_layout(config.offset_bits, get_header_size(), expected_prefix);
        if (layout != Result::SUCCESS) {
            return layout;
        }
        std::optional<E2EHeader> header_opt = msg.get_e2e_header();
        if (!header_opt.has_value() || msg.e2e_header_size() != get_header_size() ||
            msg.e2e_offset_bits() != config.offset_bits ||
            msg.e2e_unprotected_prefix_size() != expected_prefix) {
            return Result::INVALID_ARGUMENT;
        }

        const E2EHeader& header = header_opt.value();

        // Validate data ID
        if (header.data_id != config.data_id) {
            return Result::INVALID_ARGUMENT;
        }

        // Validate CRC
        if (config.enable_crc) {
            const platform::ByteBuffer crc_data = build_basic_crc_input(msg, msg.get_length());

            auto crc_result = e2ecrc::calculate_crc(crc_data, 0, crc_data.size(), config.crc_type);
            if (!crc_result.has_value()) {
                return Result::INVALID_ARGUMENT;
            }
            uint32_t expected_crc = crc_result.value();

            uint32_t received_crc = header.crc;
            if (config.crc_type == 0) {  // 8-bit
                received_crc &= 0xFFU;
                expected_crc &= 0xFFU;
            }
            else if (config.crc_type == 1) {  // 16-bit
                received_crc &= 0xFFFFU;
                expected_crc &= 0xFFFFU;
            }

            if (received_crc != expected_crc) {
                return Result::INVALID_ARGUMENT;  // CRC mismatch
            }
        }

        // Validate counter (sequence check, per data ID)
        if (config.enable_counter) {
            platform::ScopedLock const lock(counter_mutex_);
            auto it = counters_.find(config.data_id);
            if (it == counters_.end()) {
                auto [ins_it, inserted] = counters_.insert({config.data_id, 0});
                if (!inserted) {
                    return Result::RESOURCE_EXHAUSTED;
                }
                it = ins_it;
            }
            uint32_t& last_counter = it->second;

            // protect() and validate() share counters_; after protect() bumps
            // counter to N the immediate validate() will see header.counter == last_counter.
            // Equality is therefore a valid state, not a replay.

            bool counter_valid = false;

            if (last_counter == 0) {
                // First message - accept any counter >= 1
                counter_valid = (header.counter >= 1 && header.counter <= config.max_counter_value);
            }
            else if (header.counter >= last_counter) {
                counter_valid = true;
            }
            else {
                // header.counter < last_counter
                // Check if this is a rollover case
                if (last_counter > config.max_counter_value - 10) {
                    // Near rollover - allow wrap-around (counter wraps from max to 1)
                    if (header.counter >= 1 && header.counter <= 10) {
                        counter_valid = true;
                    }
                    else {
                        return Result::INVALID_ARGUMENT;  // Invalid counter (replay)
                    }
                }
                else {
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
            auto ms_now =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                    .count();
            auto current_freshness =
                static_cast<uint16_t>(static_cast<uint64_t>(ms_now) & 0xFFFFULL);

            // Calculate freshness difference (handle wrap-around)
            // Since we're using 16-bit values, we need to handle wrap-around
            // For timeout checking, we compare the lower 16 bits
            // If the difference is small (within timeout), it's fresh
            // If difference is large (close to 0xFFFF), it might be wrap-around or stale
            uint16_t freshness_diff = 0;
            if (current_freshness >= header.freshness_value) {
                freshness_diff = current_freshness - header.freshness_value;
            }
            else {
                // Wrap-around case - calculate how much time passed
                freshness_diff = static_cast<uint16_t>(
                    (0xFFFFU - static_cast<uint32_t>(header.freshness_value)) +
                    static_cast<uint32_t>(current_freshness) + 1U);
            }

            // Convert timeout to 16-bit units (approximately)
            // Since we're storing lower 16 bits of milliseconds,
            // we compare against timeout_ms directly (assuming timeout < 65535 ms)
            auto const timeout_units = static_cast<uint16_t>(
                config.freshness_timeout_ms > 0xFFFFU ? 0xFFFFU : config.freshness_timeout_ms);
            if (freshness_diff > timeout_units && freshness_diff < (0xFFFFU - timeout_units)) {
                // If difference is large and not due to wrap-around, it's stale
                return Result::TIMEOUT;  // Stale data
            }
        }

        return Result::SUCCESS;
    }

    size_t get_header_size() const override {
        return E2EHeader::get_header_size();  // 12 bytes
    }

    platform::String<> get_profile_name() const override { return platform::String<>("basic"); }

    uint32_t get_profile_id() const override {
        return 0;  // Default profile ID
    }

private:
    mutable platform::Mutex counter_mutex_;
    mutable platform::Mutex freshness_mutex_;
    platform::UnorderedMap<uint16_t, uint32_t> counters_;
    platform::UnorderedMap<uint16_t, uint16_t> freshness_values_;
};

// Initialize and register basic profile (reference implementation)
void initialize_basic_profile() {
    E2EProfileRegistry& registry = E2EProfileRegistry::instance();
    auto profile = std::make_unique<BasicE2EProfile>();
    registry.register_profile(std::move(profile));
}

// NOLINTEND(misc-include-cleaner)

}  // namespace someip::e2e
