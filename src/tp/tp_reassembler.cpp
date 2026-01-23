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
#include <algorithm>
#include <mutex>

namespace someip {
namespace tp {

/**
 * @brief SOME/IP-TP Reassembler implementation
 * @implements REQ_ARCH_001
 * @implements REQ_ARCH_002
 * @implements REQ_TP_030, REQ_TP_031, REQ_TP_032, REQ_TP_033, REQ_TP_034, REQ_TP_035
 * @implements REQ_TP_036, REQ_TP_037, REQ_TP_038, REQ_TP_039, REQ_TP_040, REQ_TP_041, REQ_TP_042, REQ_TP_043
 * @implements REQ_TP_030_E01, REQ_TP_030_E02, REQ_TP_039_E01
 * @satisfies feat_req_someiptp_410
 * @satisfies feat_req_someiptp_411
 * @satisfies feat_req_someiptp_412
 */
TpReassembler::TpReassembler(const TpConfig& config)
    : config_(config) {
}

// NOLINTNEXTLINE(modernize-use-equals-default) - intentional cleanup with lock
TpReassembler::~TpReassembler() {
    std::scoped_lock lock(buffers_mutex_);
    reassembly_buffers_.clear();
}

/**
 * @brief Parse TP header from segment payload
 * @implements REQ_TP_011, REQ_TP_012, REQ_TP_013, REQ_TP_014, REQ_TP_015
 * @implements REQ_TP_016, REQ_TP_018, REQ_TP_019, REQ_TP_020, REQ_TP_021
 * @implements REQ_TP_015_E01
 */
bool TpReassembler::parse_tp_header(const std::vector<uint8_t>& payload,
                                   uint16_t& offset, bool& more_segments) {
    if (payload.size() < 20) {  // SOME/IP header (16) + TP header (4) minimum
        return false;
    }

    // TP header starts at offset 16 (after SOME/IP header)
    uint32_t tp_header = (payload[16] << 24) | (payload[17] << 16) |
                        (payload[18] << 8) | payload[19];

    // Extract offset (28 bits, divided by 4 to get byte offset)
    uint32_t offset_units = tp_header >> 4;
    offset = offset_units * 16;  // Convert back to bytes

    // Check offset alignment (REQ_TP_015_E01)
    if (offset % 16 != 0) {
        // Log warning but continue processing
        std::cout << "Warning: Received TP segment with misaligned offset: " << offset << std::endl;
    }

    // Extract more segments flag (bit 0)
    more_segments = (tp_header & 0x01) != 0;

    // Reserved bits (bits 1-3) are ignored (REQ_TP_018)

    return true;
}

/**
 * @brief Process a received TP segment
 * @implements REQ_TP_030, REQ_TP_031, REQ_TP_032
 * @implements REQ_TP_030_E01
 */
bool TpReassembler::process_segment(const TpSegment& segment, std::vector<uint8_t>& complete_message) {
    if (!validate_segment(segment)) {
        return false;
    }

    std::scoped_lock lock(buffers_mutex_);

    TpReassemblyBuffer* buffer = find_or_create_buffer(segment);
    if (!buffer) {
        return false;
    }

    if (add_segment_to_buffer(*buffer, segment)) {
        if (buffer->is_complete()) {
            buffer->complete = true;  // Mark as complete
            complete_message = buffer->get_complete_message();
            // Remove completed buffer
            reassembly_buffers_.erase(buffer->message_id);
            return true;
        }
    }

    return true;  // Segment processed but reassembly not complete
}

/**
 * @brief Validate a TP segment
 * @implements REQ_TP_033, REQ_TP_034, REQ_TP_035
 * @implements REQ_TP_030_E02
 */
bool TpReassembler::validate_segment(const TpSegment& segment) const {
    const auto config = get_config_copy();

    // Validate segment header
    if (segment.header.segment_length != segment.payload.size()) {
        return false;
    }

    // Validate message length
    if (segment.header.message_length > config.max_message_size) {
        return false;
    }

    // Validate offset
    if (segment.header.segment_offset + segment.header.segment_length > segment.header.message_length) {
        return false;
    }

    return true;
}

/**
 * @brief Find or create reassembly buffer
 * @implements REQ_TP_036, REQ_TP_037, REQ_TP_038
 */
TpReassemblyBuffer* TpReassembler::find_or_create_buffer(const TpSegment& segment) {
    auto it = reassembly_buffers_.find(segment.header.sequence_number);

    if (it == reassembly_buffers_.end()) {
        // Create new buffer for first segment
        if (segment.header.message_type == TpMessageType::FIRST_SEGMENT ||
            segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {

            auto buffer = std::make_unique<TpReassemblyBuffer>(
                segment.header.sequence_number, segment.header.message_length);
            it = reassembly_buffers_.emplace(segment.header.sequence_number, std::move(buffer)).first;
        } else {
            // Received consecutive/last segment without first segment
            return nullptr;
        }
    }

    return it->second.get();
}

/**
 * @brief Add segment to reassembly buffer
 * @implements REQ_TP_039, REQ_TP_040, REQ_TP_041, REQ_TP_042, REQ_TP_043
 * @implements REQ_TP_039_E01
 */
bool TpReassembler::add_segment_to_buffer(TpReassemblyBuffer& buffer, const TpSegment& segment) {
    // Check if this segment was already received
    if (buffer.is_segment_received(segment.header.segment_offset, segment.header.segment_length)) {
        return true;  // Duplicate segment, ignore
    }

    // Check bounds
    if (segment.header.segment_offset + segment.header.segment_length > buffer.total_length) {
        return false;  // Segment exceeds message bounds
    }

    size_t bytes_received = 0;

    // Handle different segment types
    if (segment.header.message_type == TpMessageType::FIRST_SEGMENT) {
        // First segment contains SOME/IP header + payload data
        const size_t header_size = 16;  // SOME/IP header size
        if (segment.payload.size() > header_size) {
            bytes_received = segment.payload.size() - header_size;
            std::copy(segment.payload.begin() + header_size,
                     segment.payload.end(),
                     buffer.received_data.begin() + segment.header.segment_offset);
        }
    } else if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        // Single message contains full SOME/IP message, extract payload only
        const size_t header_size = 16;  // SOME/IP header size
        if (segment.payload.size() > header_size) {
            bytes_received = segment.payload.size() - header_size;
            std::copy(segment.payload.begin() + header_size,
                     segment.payload.end(),
                     buffer.received_data.begin());
        }
    } else {
        // Consecutive/last segments contain only payload data
        bytes_received = segment.payload.size();
        std::copy(segment.payload.begin(), segment.payload.end(),
                 buffer.received_data.begin() + segment.header.segment_offset);
    }

    // Mark the received bytes
    buffer.mark_segment_received(segment.header.segment_offset, bytes_received);

    // Update sequence tracking
    buffer.last_sequence_number = segment.header.sequence_number;

    return true;
}

bool TpReassembler::is_reassembling(uint32_t message_id) const {
    std::scoped_lock lock(buffers_mutex_);
    return reassembly_buffers_.find(message_id) != reassembly_buffers_.end();
}

bool TpReassembler::get_reassembly_progress(uint32_t message_id, uint32_t& received_bytes, uint32_t& total_bytes) const {
    const auto config = get_config_copy();

    std::scoped_lock lock(buffers_mutex_);
    auto it = reassembly_buffers_.find(message_id);

    if (it == reassembly_buffers_.end()) {
        return false;
    }

    const auto& buffer = *it->second;
    total_bytes = buffer.total_length;

    // Count received bytes
    received_bytes = 0;
    for (size_t i = 0; i < buffer.received_segments.size(); ++i) {
        if (buffer.received_segments[i]) {
            received_bytes += config.max_segment_size;  // Approximate
        }
    }

    // Adjust for last segment
    if (received_bytes > total_bytes) {
        received_bytes = total_bytes;
    }

    return true;
}

void TpReassembler::cancel_reassembly(uint32_t message_id) {
    std::scoped_lock lock(buffers_mutex_);
    reassembly_buffers_.erase(message_id);
}

void TpReassembler::process_timeouts() {
    const auto config = get_config_copy();

    std::scoped_lock lock(buffers_mutex_);
    cleanup_timed_out_buffers(config);
    cleanup_completed_buffers();
}

size_t TpReassembler::get_active_reassemblies() const {
    std::scoped_lock lock(buffers_mutex_);
    return reassembly_buffers_.size();
}

void TpReassembler::update_config(const TpConfig& config) {
    std::scoped_lock lock(config_mutex_);
    config_ = config;
}

void TpReassembler::cleanup_completed_buffers() {
    // Completed buffers are removed when reassembly finishes
}

void TpReassembler::cleanup_timed_out_buffers(const TpConfig& config) {
    auto now = std::chrono::steady_clock::now();

    for (auto it = reassembly_buffers_.begin(); it != reassembly_buffers_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second->start_time);

        if (elapsed > config.reassembly_timeout) {
            it = reassembly_buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

TpConfig TpReassembler::get_config_copy() const {
    std::scoped_lock lock(config_mutex_);
    return config_;
}

// TpReassemblyBuffer implementation
bool TpReassemblyBuffer::is_segment_received(uint16_t offset, uint16_t length) const {
    for (uint16_t i = 0; i < length; ++i) {
        size_t bit_index = offset + i;
        if (bit_index >= received_segments.size() || !received_segments[bit_index]) {
            return false;
        }
    }
    return true;
}

void TpReassemblyBuffer::mark_segment_received(uint16_t offset, uint16_t length) {
    // Ensure received_segments is large enough
    if (received_segments.size() < total_length) {
        received_segments.resize(total_length, false);
    }

    for (uint16_t i = 0; i < length; ++i) {
        size_t bit_index = offset + i;
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
    for (bool received : received_segments) {
        if (!received) {
            return false;
        }
    }

    return true;
}

std::vector<uint8_t> TpReassemblyBuffer::get_complete_message() const {
    if (!is_complete()) {
        return {};
    }
    return received_data;
}

} // namespace tp
} // namespace someip
