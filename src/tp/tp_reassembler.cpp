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

#include "tp/tp_reassembler.h"

#include "platform/buffer_pool.h"
#include "platform/thread.h"
#include "tp/tp_types.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>

namespace someip::tp {

namespace {

TpReassemblyKey make_reassembly_key(const TpSegment& segment) {
    TpReassemblyKey key;
    key.message_id = (static_cast<uint32_t>(segment.header.service_id) << 16U) |
                     static_cast<uint32_t>(segment.header.method_id);
    key.protocol_version = segment.header.protocol_version;
    key.interface_version = segment.header.interface_version;
    key.message_type = 0;
    key.request_id = segment.header.client_id;
    return key;
}

}  // namespace

/**
 * @brief SOME/IP-TP Reassembler implementation
 * @satisfies feat_req_someiptp_410
 * @satisfies feat_req_someiptp_411
 * @satisfies feat_req_someiptp_412
 */
TpReassembler::TpReassembler(const TpConfig& config)
    : config_(config) {
}

// NOLINTNEXTLINE(modernize-use-equals-default) - intentional cleanup with lock
TpReassembler::~TpReassembler() {
    platform::ScopedLock const lock(buffers_mutex_);
    reassembly_buffers_.clear();
}

/**
 * @brief Parse TP header from segment payload
 * @implements REQ_TP_011, REQ_TP_012, REQ_TP_013, REQ_TP_014, REQ_TP_015
 * @implements REQ_TP_016, REQ_TP_018, REQ_TP_019, REQ_TP_020, REQ_TP_021
 * @implements REQ_TP_015_E01
 * @implements REQ_TP_082_E01, REQ_TP_082_E02, REQ_TP_082_E03, REQ_TP_082_E04
 * @implements REQ_TP_072_E01, REQ_TP_076_E01, REQ_TP_076_E02
 * @implements REQ_TP_082
 */
bool TpReassembler::parse_tp_header(const platform::ByteBuffer& payload,
                                   uint32_t& offset, bool& more_segments) {
    if (payload.size() < 20) {  // SOME/IP header (16) + TP header (4) minimum
        return false;
    }

    // TP header starts at offset 16 (after SOME/IP header)
    uint32_t const tp_header =
        (static_cast<uint32_t>(payload[16]) << 24U) |
        (static_cast<uint32_t>(payload[17]) << 16U) |
        (static_cast<uint32_t>(payload[18]) << 8U) |
        static_cast<uint32_t>(payload[19]);

    // Extract offset (28 bits, divided by 4 to get byte offset)
    uint32_t const offset_units = tp_header >> 4U;
    offset = offset_units * 16;  // Convert back to bytes

    // Check offset alignment (REQ_TP_015_E01)
    if (offset % 16 != 0) {
        // Log warning but continue processing
        std::cout << "Warning: Received TP segment with misaligned offset: " << offset << '\n';
    }

    // Extract more segments flag (bit 0)
    more_segments = (tp_header & 0x01U) != 0;

    // Reserved bits (bits 1-3) are ignored (REQ_TP_018)

    return true;
}

/**
 * @brief Process a received TP segment
 * @implements REQ_TP_030, REQ_TP_031, REQ_TP_032
 * @implements REQ_TP_030_E01, REQ_TP_076, REQ_TP_077, REQ_TP_078
 * @implements REQ_TP_079, REQ_TP_080, REQ_TP_081, REQ_TP_082
 */
bool TpReassembler::process_segment(const TpSegment& segment, platform::ByteBuffer& complete_message) {
    if (!validate_segment(segment)) {
        return false;
    }

    platform::ScopedLock const lock(buffers_mutex_);

    const TpReassemblyKey key = make_reassembly_key(segment);

    TpReassemblyBuffer* buffer = find_or_create_buffer(segment);
    if (buffer == nullptr) {
        return false;
    }

    if (!add_segment_to_buffer(*buffer, segment)) {
        reassembly_buffers_.erase(key);
        return false;
    }

    if (buffer->is_complete()) {
        buffer->complete = true;
        complete_message = buffer->get_complete_message();
        reassembly_buffers_.erase(key);
        return true;
    }

    return true;
}

/**
 * @brief Validate a TP segment
 * @implements REQ_TP_033, REQ_TP_034, REQ_TP_035
 * @implements REQ_TP_030_E02, REQ_TP_072_E01, REQ_TP_076_E01, REQ_TP_076_E02
 */
bool TpReassembler::validate_segment(const TpSegment& segment) const {
    const auto config = get_config_copy();

    // Validate segment header: segment_length should match payload size
    if (segment.header.segment_length != segment.payload.size()) {
        return false;
    }

    // Validate message length
    if (segment.header.message_length > config.max_message_size) {
        return false;
    }

    // Calculate the actual payload bytes (excluding headers)
    // All TP segments have SOME/IP header (16 bytes) + TP header (4 bytes)
    uint16_t header_overhead = 0;
    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        header_overhead = 16;  // SOME/IP header only
    } else {
        header_overhead = 16 + 4;  // SOME/IP header + TP header for ALL TP segments
    }

    // Validate offset: the actual payload portion should fit within message bounds
    uint16_t const actual_payload_bytes = segment.header.segment_length > header_overhead
                                              ? segment.header.segment_length - header_overhead
                                              : 0;
    return segment.header.segment_offset + actual_payload_bytes <= segment.header.message_length;
}

/**
 * @brief Find or create reassembly buffer
 * @implements REQ_TP_036, REQ_TP_037, REQ_TP_038
 * @satisfies feat_req_someiptp_781, feat_req_someiptp_794, feat_req_someiptp_795, feat_req_someiptp_793
 */
TpReassemblyBuffer* TpReassembler::find_or_create_buffer(const TpSegment& segment) {
    const TpReassemblyKey key = make_reassembly_key(segment);

    auto it = reassembly_buffers_.find(key);

    if (it != reassembly_buffers_.end()) {
        // Session ID change: only restart on first/single segments (feat_req_someiptp_795, 793).
        // Mid-stream segments with a different session ID are stale; discard them.
        if (it->second.session_id != segment.header.session_id) {
            if (segment.header.message_type == TpMessageType::FIRST_SEGMENT ||
                segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
                reassembly_buffers_.erase(it);
                auto result = reassembly_buffers_.insert(
                    std::make_pair(key,
                        TpReassemblyBuffer(key.message_id, segment.header.message_length,
                                           segment.header.session_id)));
                return &result.first->second;
            }
            return nullptr;
        }
        return &it->second;
    }

    if (segment.header.message_type == TpMessageType::FIRST_SEGMENT ||
        segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        const auto config = get_config_copy();
        if (reassembly_buffers_.size() >= config.max_concurrent_transfers) {
            return nullptr;
        }
        auto result = reassembly_buffers_.insert(
            std::make_pair(key,
                TpReassemblyBuffer(key.message_id, segment.header.message_length,
                                   segment.header.session_id)));
        return &result.first->second;
    }

    return nullptr;
}

/**
 * @brief Add segment to reassembly buffer
 * @implements REQ_TP_039, REQ_TP_040, REQ_TP_041, REQ_TP_042, REQ_TP_043
 * @implements REQ_TP_039_E01, REQ_TP_080, REQ_TP_081
 */
bool TpReassembler::add_segment_to_buffer(TpReassemblyBuffer& buffer, const TpSegment& segment) {
    // Calculate actual payload bytes (excluding headers)
    size_t header_overhead = 0;
    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        header_overhead = 16;  // SOME/IP header only
    } else {
        header_overhead = 16 + 4;  // SOME/IP header + TP header for ALL TP segments
    }

    size_t const actual_payload_bytes = segment.payload.size() > header_overhead
                                  ? segment.payload.size() - header_overhead
                                  : 0;

    // Check if this segment was already received
    if (buffer.is_segment_received(segment.header.segment_offset, actual_payload_bytes)) {
        return true;  // Duplicate segment, ignore
    }

    // Check bounds using actual payload bytes
    if (segment.header.segment_offset + actual_payload_bytes > buffer.total_length) {
        return false;  // Segment exceeds message bounds
    }

    size_t bytes_received = 0;

    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        const size_t header_size = 16;
        if (segment.payload.size() > header_size) {
            bytes_received = segment.payload.size() - header_size;
            std::copy(segment.payload.begin() + static_cast<std::ptrdiff_t>(header_size),
                     segment.payload.end(),
                     buffer.received_data.begin());
        }
    } else {
        // All TP segments (FIRST, CONSECUTIVE, LAST): 16 SOME/IP + 4 TP header
        constexpr size_t tp_header_size = 16 + 4;
        if (segment.payload.size() > tp_header_size) {
            bytes_received = segment.payload.size() - tp_header_size;
            std::copy(segment.payload.begin() + static_cast<std::ptrdiff_t>(tp_header_size),
                     segment.payload.end(),
                     buffer.received_data.begin() + segment.header.segment_offset);
        }
    }

    // Mark the received bytes
    buffer.mark_segment_received(segment.header.segment_offset, bytes_received);

    // Update sequence tracking
    buffer.last_sequence_number = segment.header.sequence_number;

    return true;
}

bool TpReassembler::is_reassembling(uint32_t message_id) const {
    platform::ScopedLock const lock(buffers_mutex_);
    for (const auto& pair : reassembly_buffers_) {
        if (pair.first.message_id == message_id) {
            return true;
        }
    }
    return false;
}

bool TpReassembler::get_reassembly_progress(uint32_t message_id, uint32_t& received_bytes, uint32_t& total_bytes) const {
    const auto config = get_config_copy();

    platform::ScopedLock const lock(buffers_mutex_);

    for (const auto& pair : reassembly_buffers_) {
        if (pair.first.message_id == message_id) {
            const auto& buffer = pair.second;
            total_bytes = buffer.total_length;

            received_bytes = 0;
            for (bool const received : buffer.received_segments) {
                if (received) {
                    received_bytes += config.max_segment_size;
                }
            }

            if (received_bytes > total_bytes) {
                received_bytes = total_bytes;
            }

            return true;
        }
    }

    return false;
}

/**
 * @brief Cancel reassembly for a message
 * @implements REQ_TP_079
 */
void TpReassembler::cancel_reassembly(uint32_t message_id) {
    platform::ScopedLock const lock(buffers_mutex_);
    for (auto it = reassembly_buffers_.begin(); it != reassembly_buffers_.end(); ) {
        if (it->first.message_id == message_id) {
            it = reassembly_buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * @brief Process reassembly timeouts
 * @implements REQ_TP_079
 */
void TpReassembler::process_timeouts() {
    const auto config = get_config_copy();

    platform::ScopedLock const lock(buffers_mutex_);
    cleanup_timed_out_buffers(config);
    cleanup_completed_buffers();
}

size_t TpReassembler::get_active_reassemblies() const {
    platform::ScopedLock const lock(buffers_mutex_);
    return reassembly_buffers_.size();
}

void TpReassembler::update_config(const TpConfig& config) {
    platform::ScopedLock const lock(config_mutex_);
    config_ = config;
}

void TpReassembler::cleanup_completed_buffers() {
    // Completed buffers are removed when reassembly finishes
}

void TpReassembler::cleanup_timed_out_buffers(const TpConfig& config) {
    auto const now = std::chrono::steady_clock::now();

    for (auto it = reassembly_buffers_.begin(); it != reassembly_buffers_.end(); ) {
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.start_time);

        if (elapsed > config.reassembly_timeout) {
            it = reassembly_buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

TpConfig TpReassembler::get_config_copy() const {
    platform::ScopedLock const lock(config_mutex_);
    return config_;
}

// TpReassemblyBuffer implementation
bool TpReassemblyBuffer::is_segment_received(uint32_t offset, uint32_t length) const {
    for (uint32_t i = 0; i < length; ++i) {
        size_t const bit_index = offset + i;
        if (bit_index >= received_segments.size() || !received_segments[bit_index]) {
            return false;
        }
    }
    return true;
}

void TpReassemblyBuffer::mark_segment_received(uint32_t offset, uint32_t length) {
    if (received_segments.size() < total_length) {
        received_segments.resize(total_length, false);
    }

    for (uint32_t i = 0; i < length; ++i) {
        size_t const bit_index = offset + i;
        if (bit_index < received_segments.size()) {
            received_segments[bit_index] = true;
        }
    }
}

bool TpReassemblyBuffer::is_complete() const {
    if (complete) {
        return true;
    }

    // Check if all segments received
    for (bool const received : received_segments) {
        if (!received) {
            return false;
        }
    }

    return true;
}

platform::ByteBuffer TpReassemblyBuffer::get_complete_message() const {
    if (!is_complete()) {
        return {};
    }
    return received_data;
}

}  // namespace someip::tp
