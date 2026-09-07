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
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

namespace {

constexpr size_t kSomeipHeaderSize = 16;
constexpr size_t kTpOverhead = kSomeipHeaderSize + 4;

uint16_t tp_payload_bytes(const TpSegment& segment) {
    if (segment.payload.size() <= kTpOverhead) {
        return 0;
    }
    return static_cast<uint16_t>(segment.payload.size() - kTpOverhead);
}

void store_someip_header(TpReassemblyBuffer& buffer, const TpSegment& segment, bool last_segment) {
    if (segment.payload.size() < kSomeipHeaderSize) {
        return;
    }
    if (!buffer.has_someip_header || last_segment) {
        std::memcpy(buffer.someip_header.data(), segment.payload.data(), kSomeipHeaderSize);
        buffer.has_someip_header = true;
    }
}

}  // namespace

/**
 * @brief Parse TP header from a raw datagram
 * @implements REQ_TP_011, REQ_TP_012, REQ_TP_013, REQ_TP_014, REQ_TP_015
 * @implements REQ_TP_016, REQ_TP_018, REQ_TP_019, REQ_TP_020, REQ_TP_021
 * @implements REQ_TP_015_E01
 * @implements REQ_TP_082_E01, REQ_TP_082_E02, REQ_TP_082_E03, REQ_TP_082_E04
 * @implements REQ_TP_072_E01, REQ_TP_076_E01, REQ_TP_076_E02
 * @implements REQ_TP_082
 */
bool parse_tp_header(const uint8_t* data, size_t size, uint32_t& offset, bool& more_segments) {
    if (data == nullptr || size < kTpOverhead) {
        return false;
    }

    uint32_t const tp_header =
        (static_cast<uint32_t>(data[16]) << 24U) |
        (static_cast<uint32_t>(data[17]) << 16U) |
        (static_cast<uint32_t>(data[18]) << 8U) |
        static_cast<uint32_t>(data[19]);

    uint32_t const offset_units = tp_header >> 4U;
    offset = offset_units * 16U;

    if (offset % 16U != 0U) {
        std::cout << "Warning: Received TP segment with misaligned offset: " << offset << '\n';
    }

    more_segments = (tp_header & 0x01U) != 0;
    return true;
}

bool parse_wire_segment(const uint8_t* data, size_t size, TpSegment& out_segment) {
    if (data == nullptr || size < kTpOverhead || size > UINT16_MAX) {
        return false;
    }
    if ((data[14] & 0x20U) == 0U) {
        return false;
    }

    uint32_t offset = 0;
    bool more = false;
    if (!parse_tp_header(data, size, offset, more)) {
        return false;
    }

    out_segment = TpSegment();
    out_segment.payload.resize(size);
    if (size > 0) {
        std::memcpy(out_segment.payload.data(), data, size);
    }
    out_segment.header.segment_length = static_cast<uint16_t>(size);
    out_segment.header.segment_offset = offset;
    out_segment.header.message_length = 0;  // wire TP header has no total size

    const auto service = static_cast<uint16_t>(
        (static_cast<unsigned>(data[0]) << 8U) | static_cast<unsigned>(data[1]));
    const auto method = static_cast<uint16_t>(
        (static_cast<unsigned>(data[2]) << 8U) | static_cast<unsigned>(data[3]));
    const auto client = static_cast<uint16_t>(
        (static_cast<unsigned>(data[8]) << 8U) | static_cast<unsigned>(data[9]));
    const auto session = static_cast<uint16_t>(
        (static_cast<unsigned>(data[10]) << 8U) | static_cast<unsigned>(data[11]));
    out_segment.header.service_id = service;
    out_segment.header.method_id = method;
    out_segment.header.client_id = client;
    out_segment.header.session_id = session;
    out_segment.header.protocol_version = data[12];
    out_segment.header.interface_version = data[13];

    if (offset == 0 && more) {
        out_segment.header.message_type = TpMessageType::FIRST_SEGMENT;
    } else if (!more && offset > 0) {
        out_segment.header.message_type = TpMessageType::LAST_SEGMENT;
    } else if (more && offset > 0) {
        out_segment.header.message_type = TpMessageType::CONSECUTIVE_SEGMENT;
    } else {
        // offset == 0 && !more: single TP segment
        out_segment.header.message_type = TpMessageType::FIRST_SEGMENT;
    }

    return true;
}

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
 * @brief Process a received TP segment
 * @implements REQ_TP_030, REQ_TP_031, REQ_TP_032, REQ_TP_033
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

    // When the sender populated message_length (internal segmenter path),
    // reject follow-up segments that disagree. Wire ingest leaves
    // message_length at 0 because the TP header has no total size.
    if (segment.header.message_length > 0 && buffer->known_total &&
        segment.header.message_length != buffer->total_length) {
        return false;
    }

    const auto config = get_config_copy();
    if (!add_segment_to_buffer(*buffer, segment, config.max_message_size)) {
        return false;
    }

    if (buffer->is_complete()) {
        buffer->complete = true;
        complete_message = buffer->get_complete_message();
        if (buffer->has_someip_header) {
            last_completed_someip_header_ = buffer->someip_header;
            has_last_completed_someip_header_ = true;
        }
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

    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        if (segment.header.message_length > 0) {
            return actual_payload_bytes <= segment.header.message_length;
        }
        return true;
    }

    uint32_t wire_offset = 0;
    bool wire_more = false;
    if (!parse_tp_header(segment.payload.data(), segment.payload.size(), wire_offset, wire_more)) {
        return false;
    }
    if (wire_offset > config.max_message_size) {
        return false;
    }
    if (actual_payload_bytes > config.max_message_size - wire_offset) {
        return false;
    }
    // Known total (internal segmenter path) still bounds-checks against message_length.
    if (segment.header.message_length > 0) {
        if (wire_offset > segment.header.message_length) {
            return false;
        }
        return actual_payload_bytes <= segment.header.message_length - wire_offset;
    }
    return true;
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

    // FIRST (offset 0) and LAST (More=0) may open a buffer so last-arrives-first
    // works. CONSECUTIVE (offset>0 and More=1) must attach to an existing buffer.
    if (segment.header.message_type != TpMessageType::FIRST_SEGMENT &&
        segment.header.message_type != TpMessageType::SINGLE_MESSAGE &&
        segment.header.message_type != TpMessageType::LAST_SEGMENT) {
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

    uint32_t initial_length = segment.header.message_length;
    if (initial_length == 0 && segment.header.message_type != TpMessageType::SINGLE_MESSAGE) {
        uint32_t wire_offset = 0;
        bool wire_more = false;
        if (parse_tp_header(segment.payload.data(), segment.payload.size(), wire_offset, wire_more)) {
            initial_length = wire_offset + tp_payload_bytes(segment);
        }
    }

    auto result = reassembly_buffers_.insert(
        std::make_pair(key,
            TpReassemblyBuffer(key.message_id, initial_length, segment.header.session_id)));
    if (segment.header.message_length == 0) {
        result.first->second.known_total = false;
    }
    return &result.first->second;
}

/**
 * @brief Add segment to reassembly buffer
 * @implements REQ_TP_039, REQ_TP_040, REQ_TP_041, REQ_TP_042, REQ_TP_043
 * @implements REQ_TP_039_E01, REQ_TP_080, REQ_TP_081
 */
bool TpReassembler::add_segment_to_buffer(TpReassemblyBuffer& buffer, const TpSegment& segment,
                                          uint32_t max_message_size) {
    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        constexpr size_t header_size = 16;
        if (segment.payload.size() <= header_size) {
            return false;
        }
        const size_t bytes = segment.payload.size() - header_size;

        if (buffer.is_segment_received(0, static_cast<uint32_t>(bytes))) {
            return true;
        }
        if (buffer.known_total && bytes > buffer.total_length) {
            return false;
        }
        if (!buffer.ensure_size(static_cast<uint32_t>(bytes), max_message_size)) {
            return false;
        }

        std::copy(segment.payload.begin() + static_cast<std::ptrdiff_t>(header_size),
                 segment.payload.end(),
                 buffer.received_data.begin());
        buffer.mark_segment_received(0, static_cast<uint32_t>(bytes));
        buffer.last_sequence_number = segment.header.sequence_number;
        buffer.last_segment_seen = true;
        buffer.finalized_length = static_cast<uint32_t>(bytes);
        store_someip_header(buffer, segment, true);
        return true;
    }

    if (segment.payload.size() <= kTpOverhead) {
        return false;
    }

    uint32_t wire_offset = 0;
    bool wire_more = false;
    if (!parse_tp_header(segment.payload.data(), segment.payload.size(), wire_offset, wire_more)) {
        return false;
    }

    const auto bytes = static_cast<uint32_t>(segment.payload.size() - kTpOverhead);

    if (buffer.is_segment_received(wire_offset, bytes)) {
        return true;
    }

    if (buffer.last_segment_seen &&
        (wire_offset > buffer.finalized_length ||
         bytes > buffer.finalized_length - wire_offset)) {
        return false;
    }

    if (buffer.known_total) {
        if (wire_offset > buffer.total_length ||
            bytes > buffer.total_length - wire_offset) {
            return false;
        }
    } else {
        if (wire_offset > max_message_size || bytes > max_message_size - wire_offset) {
            return false;
        }
        if (!buffer.ensure_size(wire_offset + bytes, max_message_size)) {
            return false;
        }
    }

    if (!wire_more) {
        const uint32_t implied = wire_offset + bytes;
        if (buffer.last_segment_seen && buffer.finalized_length != implied) {
            return false;
        }
        if (buffer.known_total && implied != buffer.total_length) {
            return false;
        }
        buffer.last_segment_seen = true;
        buffer.finalized_length = implied;
        if (!buffer.known_total) {
            buffer.total_length = implied;
        }
    }

    std::copy(segment.payload.begin() + static_cast<std::ptrdiff_t>(kTpOverhead),
             segment.payload.end(),
             buffer.received_data.begin() + static_cast<std::ptrdiff_t>(wire_offset));
    buffer.mark_segment_received(wire_offset, bytes);
    buffer.last_sequence_number = segment.header.sequence_number;
    store_someip_header(buffer, segment, !wire_more);
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

bool TpReassembler::copy_last_completed_someip_header(std::array<uint8_t, 16>& out) const {
    platform::ScopedLock const lock(buffers_mutex_);
    if (!has_last_completed_someip_header_) {
        return false;
    }
    out = last_completed_someip_header_;
    return true;
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

    if (!last_segment_seen || finalized_length == 0) {
        return false;
    }

    if (received_segments.size() < finalized_length) {
        return false;
    }

    for (uint32_t i = 0; i < finalized_length; ++i) {
        if (!received_segments[i]) {
            return false;
        }
    }

    return true;
}

platform::ByteBuffer TpReassemblyBuffer::get_complete_message() const {
    if (!is_complete()) {
        return {};
    }
    if (received_data.size() < finalized_length || received_data.data() == nullptr) {
        return {};
    }
    platform::ByteBuffer result(finalized_length);
    if (result.size() < finalized_length || result.data() == nullptr) {
        return {};
    }
    std::memcpy(result.data(), received_data.data(), finalized_length);
    return result;
}

bool TpReassemblyBuffer::ensure_size(uint32_t needed, uint32_t max_message_size) {
    if (needed == 0 || needed > max_message_size) {
        return false;
    }
    if (received_data.size() < needed) {
        received_data.resize(needed);
        if (received_data.size() < needed) {
            return false;
        }
    }
    if (received_segments.size() < needed) {
        received_segments.resize(needed, false);
    }
    if (total_length < needed) {
        total_length = needed;
    }
    return true;
}

}  // namespace someip::tp
