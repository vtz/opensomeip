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

/**
 * Build the spec-mandated reassembly key from the wire payload.
 * Key = Message ID + Protocol Version + Interface Version
 *     + Message Type (SOME/IP byte 14, TP-flag masked off)
 *     + Request ID (Client ID << 16 | Session ID)
 * @satisfies feat_req_someiptp_781
 */
TpReassemblyKey make_reassembly_key(const TpSegment& segment) {
    TpReassemblyKey key;

    // Parse ALL key fields from the wire 16-byte SOME/IP header when available,
    // so a peer datagram stored in segment.payload keys correctly regardless of
    // whether TpSegmentHeader was populated.
    if (segment.payload.size() >= 16) {
        const auto* p = segment.payload.data();
        const auto service = static_cast<uint16_t>((static_cast<unsigned>(p[0]) << 8U) | static_cast<unsigned>(p[1]));
        const auto method  = static_cast<uint16_t>((static_cast<unsigned>(p[2]) << 8U) | static_cast<unsigned>(p[3]));
        const auto client  = static_cast<uint16_t>((static_cast<unsigned>(p[8]) << 8U) | static_cast<unsigned>(p[9]));
        const auto session = static_cast<uint16_t>((static_cast<unsigned>(p[10]) << 8U) | static_cast<unsigned>(p[11]));

        key.message_id = (static_cast<uint32_t>(service) << 16U) | method;
        key.protocol_version = p[12];
        key.interface_version = p[13];
        key.message_type = p[14] & static_cast<uint8_t>(~0x20U);
        key.request_id = (static_cast<uint32_t>(client) << 16U) | session;
    } else {
        // Fallback for payloads too short (rejected by validate_segment for TP).
        key.message_id = (static_cast<uint32_t>(segment.header.service_id) << 16U) |
                         static_cast<uint32_t>(segment.header.method_id);
        key.protocol_version = segment.header.protocol_version;
        key.interface_version = segment.header.interface_version;
        key.message_type = 0;
        key.request_id = (static_cast<uint32_t>(segment.header.client_id) << 16U) |
                         static_cast<uint32_t>(segment.header.session_id);
    }
    return key;
}

}  // namespace

/**
 * @brief SOME/IP-TP Reassembler implementation
 * @satisfies feat_req_someiptp_410
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
                                   uint32_t& offset, bool& more_segments) const {
    if (payload.size() < 20) {  // SOME/IP header (16) + TP header (4) minimum
        return false;
    }

    // TP header starts at offset 16 (after SOME/IP header)
    uint32_t const tp_header =
        (static_cast<uint32_t>(payload[16]) << 24U) |
        (static_cast<uint32_t>(payload[17]) << 16U) |
        (static_cast<uint32_t>(payload[18]) << 8U) |
        static_cast<uint32_t>(payload[19]);

    // Upper 28 bits = byte_offset / 16; shift right by 4 to extract, then * 16 to get bytes.
    uint32_t const offset_units = tp_header >> 4U;
    offset = offset_units * 16;

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

    // Reject follow-up segments whose message_length disagrees with the
    // buffer created by the FIRST segment.  A rogue segment with a larger
    // message_length would pass validate_segment (which uses the segment's
    // own message_length) but could exceed the buffer allocation.
    if (segment.header.message_length != buffer->total_length) {
        return false;
    }

    if (!add_segment_to_buffer(*buffer, segment)) {
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

    if (segment.header.segment_length != segment.payload.size()) {
        return false;
    }

    if (segment.header.message_length > config.max_message_size) {
        return false;
    }

    const uint16_t header_overhead =
        (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) ? 16 : 20;

    if (segment.header.segment_length < header_overhead) {
        return false;
    }

    const uint16_t actual_payload_bytes = segment.header.segment_length - header_overhead;

    if (actual_payload_bytes == 0) {
        return false;
    }

    // Use the wire TP offset for bounds checking so validation agrees with
    // placement in add_segment_to_buffer (which also uses the wire offset).
    // Overflow-safe: split into two comparisons to avoid uint32_t wrap.
    if (segment.header.message_type != TpMessageType::SINGLE_MESSAGE) {
        uint32_t wire_offset = 0;
        bool wire_more = false;
        if (!parse_tp_header(segment.payload, wire_offset, wire_more)) {
            return false;
        }
        if (wire_offset > segment.header.message_length) {
            return false;
        }
        return actual_payload_bytes <= segment.header.message_length - wire_offset;
    }

    return actual_payload_bytes <= segment.header.message_length;
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
        return &it->second;
    }

    // No exact key match (includes Session ID).
    // Only FIRST/SINGLE may create a new buffer.
    if (segment.header.message_type != TpMessageType::FIRST_SEGMENT &&
        segment.header.message_type != TpMessageType::SINGLE_MESSAGE) {
        return nullptr;
    }

    // Stale detection (feat_req_someiptp_795, 793): discard any existing buffer
    // that matches on everything except Session ID (same Client ID, different Session).
    const auto client_id_bits = static_cast<uint16_t>(key.request_id >> 16U);
    for (auto stale = reassembly_buffers_.begin(); stale != reassembly_buffers_.end(); ++stale) {
        if (stale->first.message_id == key.message_id &&
            stale->first.protocol_version == key.protocol_version &&
            stale->first.interface_version == key.interface_version &&
            stale->first.message_type == key.message_type &&
            static_cast<uint16_t>(stale->first.request_id >> 16U) == client_id_bits &&
            stale->first.request_id != key.request_id) {
            reassembly_buffers_.erase(stale);
            break;
        }
    }

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

/**
 * @brief Add segment to reassembly buffer
 * @implements REQ_TP_039, REQ_TP_040, REQ_TP_041, REQ_TP_042, REQ_TP_043
 * @implements REQ_TP_039_E01, REQ_TP_080, REQ_TP_081
 */
bool TpReassembler::add_segment_to_buffer(TpReassemblyBuffer& buffer, const TpSegment& segment) {
    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        constexpr size_t header_size = 16;
        if (segment.payload.size() <= header_size) {
            return false;
        }
        const size_t bytes = segment.payload.size() - header_size;

        if (buffer.is_segment_received(0, bytes)) {
            return true;
        }
        if (bytes > buffer.total_length) {
            return false;
        }

        std::copy(segment.payload.begin() + static_cast<std::ptrdiff_t>(header_size),
                 segment.payload.end(),
                 buffer.received_data.begin());
        buffer.mark_segment_received(0, bytes);
        buffer.last_sequence_number = segment.header.sequence_number;
        return true;
    }

    // TP segments: parse wire TP offset from payload bytes [16..19].
    constexpr size_t tp_header_size = 16 + 4;
    if (segment.payload.size() <= tp_header_size) {
        return false;
    }

    uint32_t wire_offset = 0;
    bool wire_more = false;
    if (!parse_tp_header(segment.payload, wire_offset, wire_more)) {
        return false;
    }

    const size_t bytes = segment.payload.size() - tp_header_size;

    if (buffer.is_segment_received(wire_offset, bytes)) {
        return true;
    }
    if (wire_offset > buffer.total_length ||
        bytes > static_cast<size_t>(buffer.total_length - wire_offset)) {
        return false;
    }

    std::copy(segment.payload.begin() + static_cast<std::ptrdiff_t>(tp_header_size),
             segment.payload.end(),
             buffer.received_data.begin() + wire_offset);
    buffer.mark_segment_received(wire_offset, bytes);
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

bool TpReassembler::is_reassembling(const TpReassemblyKey& key) const {
    platform::ScopedLock const lock(buffers_mutex_);
    return reassembly_buffers_.find(key) != reassembly_buffers_.end();
}

bool TpReassembler::get_reassembly_progress(uint32_t message_id, uint32_t& received_bytes, uint32_t& total_bytes) const {
    platform::ScopedLock const lock(buffers_mutex_);

    for (const auto& pair : reassembly_buffers_) {
        if (pair.first.message_id == message_id) {
            const auto& buffer = pair.second;
            total_bytes = buffer.total_length;

            // received_segments is a per-byte bitmap; count set bits.
            received_bytes = 0;
            for (bool const received : buffer.received_segments) {
                if (received) {
                    ++received_bytes;
                }
            }

            return true;
        }
    }

    return false;
}

bool TpReassembler::get_reassembly_progress(const TpReassemblyKey& key, uint32_t& received_bytes, uint32_t& total_bytes) const {
    platform::ScopedLock const lock(buffers_mutex_);

    auto it = reassembly_buffers_.find(key);
    if (it == reassembly_buffers_.end()) {
        return false;
    }

    const auto& buffer = it->second;
    total_bytes = buffer.total_length;

    received_bytes = 0;
    for (bool const received : buffer.received_segments) {
        if (received) {
            ++received_bytes;
        }
    }

    return true;
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

void TpReassembler::cancel_reassembly(const TpReassemblyKey& key) {
    platform::ScopedLock const lock(buffers_mutex_);
    reassembly_buffers_.erase(key);
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
    if (length == 0) {
        return false;
    }
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

    if (total_length == 0 || received_segments.size() < total_length) {
        return false;
    }

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
