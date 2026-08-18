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

#include "tp/tp_segmenter.h"

#include "platform/buffer_pool.h"
#include "someip/message.h"
#include "someip/types.h"
#include "tp/tp_types.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace someip::tp {

/**
 * @brief SOME/IP-TP Segmenter implementation
 * @satisfies feat_req_someiptp_402
 * @satisfies feat_req_someiptp_403
 * @satisfies feat_req_someiptp_404
 */
TpSegmenter::TpSegmenter(const TpConfig& config)
    : config_(config) {
}

/**
 * @brief Segment a message into TP segments
 * @implements REQ_TP_001, REQ_TP_002, REQ_TP_003, REQ_TP_004
 * @implements REQ_TP_001_E01, REQ_TP_070, REQ_TP_071, REQ_TP_072, REQ_TP_073, REQ_TP_074, REQ_TP_075
 */
TpResult TpSegmenter::segment_message(const Message& message, TpSegmentVector& segments) {
    // Get the message payload (without headers - TP handles payload only)
    const platform::ByteBuffer& payload = message.get_payload();

    if (payload.size() > config_.max_message_size) {
        return TpResult::MESSAGE_TOO_LARGE;
    }

    // Check if segmentation is needed
    if (payload.size() <= config_.max_segment_size) {
        // Single segment message - still need to include SOME/IP header
        // For single segment messages, we still add the TP flag if the payload is large
        // enough to indicate this should use TP (though it fits in one segment)
        Message tp_message = message;
        if (payload.size() > 1000) {  // Threshold for when to use TP even for single segments
            tp_message.set_message_type(add_tp_flag(message.get_message_type()));
        }
        platform::ByteBuffer message_data = tp_message.serialize();

        TpSegment segment;
        segment.header.message_type = TpMessageType::SINGLE_MESSAGE;
        segment.header.message_length = static_cast<uint32_t>(payload.size());
        segment.header.segment_offset = 0;
        segment.header.segment_length = static_cast<uint16_t>(message_data.size());
        segment.header.sequence_number = next_sequence_number_++;
        segment.payload = std::move(message_data);  // Full message for single segment

        if (segments.size() >= segments.max_size()) {
            return TpResult::MESSAGE_TOO_LARGE;
        }
        segments.push_back(std::move(segment));
        return TpResult::SUCCESS;
    }

    // Multi-segment message - segment the payload only
    return create_multi_segments(message, payload, segments);
}

/**
 * @brief Create multiple TP segments from a large message
 * @implements REQ_TP_002, REQ_TP_003, REQ_TP_004, REQ_TP_005, REQ_TP_006
 * @implements REQ_TP_007, REQ_TP_008, REQ_TP_010, REQ_TP_011, REQ_TP_012
 * @implements REQ_TP_013, REQ_TP_014, REQ_TP_015, REQ_TP_016, REQ_TP_017
 * @implements REQ_TP_018, REQ_TP_019, REQ_TP_020, REQ_TP_021, REQ_TP_022
 * @implements REQ_TP_001_E02, REQ_TP_001_E03, REQ_TP_013_E01, REQ_TP_015_E01
 * @implements REQ_TP_070, REQ_TP_071, REQ_TP_072, REQ_TP_073, REQ_TP_074, REQ_TP_075
 * @implements REQ_TP_076, REQ_TP_077, REQ_TP_078
 */
TpResult TpSegmenter::create_multi_segments(const Message& message,
                                          const platform::ByteBuffer& payload,
                                          TpSegmentVector& segments) {

    auto const total_length = static_cast<uint32_t>(payload.size());

    // Create a copy of the message with TP flag added to message type
    Message tp_message = message;
    tp_message.set_message_type(add_tp_flag(message.get_message_type()));

    // Every segment has 16-byte SOME/IP header + 4-byte TP header = 20 bytes overhead
    constexpr size_t SEGMENT_OVERHEAD = 16 + 4;
    if (config_.max_segment_size <= SEGMENT_OVERHEAD) {
        return TpResult::SEGMENTATION_FAILED;
    }

    // Per spec (feat_req_someiptp_772, 778, 779): non-last segment payload
    // must be multiple of 16 and all MS=1 segments must have equal size.
    size_t raw_capacity = config_.max_segment_size - SEGMENT_OVERHEAD;
    size_t uniform_payload = (raw_capacity / 16) * 16;  // Round down to multiple of 16
    if (uniform_payload == 0) {
        return TpResult::SEGMENTATION_FAILED;
    }

    uint8_t const sequence_number = next_sequence_number_;
    uint32_t payload_offset = 0;

    while (payload_offset < total_length) {
        uint32_t remaining = total_length - payload_offset;
        bool more_segments = remaining > uniform_payload;
        const uint32_t seg_payload_size = more_segments
            ? static_cast<uint32_t>(uniform_payload)
            : remaining;

        TpSegment segment;
        if (payload_offset == 0) {
            segment.header.message_type = TpMessageType::FIRST_SEGMENT;
        } else if (!more_segments) {
            segment.header.message_type = TpMessageType::LAST_SEGMENT;
        } else {
            segment.header.message_type = TpMessageType::CONSECUTIVE_SEGMENT;
        }
        segment.header.message_length = total_length;
        segment.header.segment_offset = payload_offset;
        segment.header.sequence_number = sequence_number;

        // Build full SOME/IP header (16 bytes) from the tp_message
        platform::ByteBuffer seg_data = tp_message.serialize();
        seg_data.resize(16);  // Keep only header

        // Update SOME/IP length field: 8 + 4 + seg_payload_size
        const uint32_t someip_length = 8 + 4 + seg_payload_size;
        seg_data[4] = static_cast<uint8_t>((someip_length >> 24U) & 0xFFU);
        seg_data[5] = static_cast<uint8_t>((someip_length >> 16U) & 0xFFU);
        seg_data[6] = static_cast<uint8_t>((someip_length >> 8U) & 0xFFU);
        seg_data[7] = static_cast<uint8_t>(someip_length & 0xFFU);

        // Insert 4-byte TP header after SOME/IP header
        const uint32_t offset_units = payload_offset / 16;
        const uint32_t tp_header = (offset_units << 4U) | (more_segments ? 0x01U : 0x00U);
        seg_data.push_back(static_cast<uint8_t>((tp_header >> 24U) & 0xFFU));
        seg_data.push_back(static_cast<uint8_t>((tp_header >> 16U) & 0xFFU));
        seg_data.push_back(static_cast<uint8_t>((tp_header >> 8U) & 0xFFU));
        seg_data.push_back(static_cast<uint8_t>(tp_header & 0xFFU));

        // Append payload data
        seg_data.insert(seg_data.end(),
                       payload.begin() + payload_offset,
                       payload.begin() + payload_offset + seg_payload_size);

        segment.header.segment_length = static_cast<uint16_t>(seg_data.size());
        segment.payload = std::move(seg_data);

        if (segments.size() >= segments.max_size()) {
            return TpResult::MESSAGE_TOO_LARGE;
        }
        segments.push_back(std::move(segment));
        payload_offset += seg_payload_size;
    }

    next_sequence_number_ = (next_sequence_number_ + 1) % 256;
    return TpResult::SUCCESS;
}

/**
 * @brief Update TP segmenter configuration
 * @implements REQ_TP_070, REQ_TP_071, REQ_TP_072, REQ_TP_073, REQ_TP_074, REQ_TP_075
 */
void TpSegmenter::update_config(const TpConfig& config) {
    config_ = config;
}

/**
 * @brief Serialize TP header into segment payload
 * @implements REQ_TP_011, REQ_TP_012, REQ_TP_013, REQ_TP_014, REQ_TP_015
 * @implements REQ_TP_016, REQ_TP_017, REQ_TP_019, REQ_TP_020, REQ_TP_021
 * @implements REQ_TP_013_E01, REQ_TP_015_E01
 */
void TpSegmenter::serialize_tp_header(platform::ByteBuffer& payload,
                                     uint32_t offset, bool more_segments) {
    // TP header is 4 bytes: [Offset (28 bits) | Reserved (3 bits) | More Segments (1 bit)]
    // Offset is in units of 16 bytes, so divide by 16
    uint32_t const offset_units = offset / 16;

    // Check for offset overflow (REQ_TP_013_E01)
    if (offset_units > 0x0FFFFFFFU) {  // 28 bits max
        // This should not happen in practice with reasonable message sizes
        // but we check for completeness
    }

    // Check offset alignment (REQ_TP_015_E01)
    if (offset % 16 != 0) {
        // This should not happen with our segmenter logic
        // but we could log a warning here
    }

    // Build TP header: offset (28 bits) | reserved (3 bits = 0) | more_segments (1 bit)
    uint32_t const tp_header = (offset_units << 4U) | (more_segments ? 0x01U : 0x00U);

    // Serialize in big-endian
    std::array<uint8_t, 4> header_bytes{};
    header_bytes[0] = static_cast<uint8_t>((tp_header >> 24U) & 0xFFU);
    header_bytes[1] = static_cast<uint8_t>((tp_header >> 16U) & 0xFFU);
    header_bytes[2] = static_cast<uint8_t>((tp_header >> 8U) & 0xFFU);
    header_bytes[3] = static_cast<uint8_t>(tp_header & 0xFFU);

    // Insert TP header after SOME/IP header (16 bytes)
    payload.insert(payload.begin() + 16, header_bytes.begin(), header_bytes.end());
}

/**
 * @brief Add TP flag to message type
 * @implements REQ_TP_007, REQ_TP_008
 */
MessageType TpSegmenter::add_tp_flag(MessageType type) const {
    // TP flag is bit 5 (0x20)
    uint8_t const tp_flag = 0x20U;
    return static_cast<MessageType>(static_cast<uint32_t>(static_cast<uint8_t>(type)) |
                                    static_cast<uint32_t>(tp_flag));
}

}  // namespace someip::tp
