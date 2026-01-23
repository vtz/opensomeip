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
#include "someip/message.h"
#include <algorithm>
#include <iostream>

namespace someip {
namespace tp {

/**
 * @brief SOME/IP-TP Segmenter implementation
 * @implements REQ_ARCH_001
 * @implements REQ_TP_001, REQ_TP_002, REQ_TP_003, REQ_TP_004, REQ_TP_005, REQ_TP_006, REQ_TP_007, REQ_TP_008
 * @implements REQ_TP_010, REQ_TP_011, REQ_TP_012, REQ_TP_013, REQ_TP_014, REQ_TP_015, REQ_TP_016, REQ_TP_017
 * @implements REQ_TP_018, REQ_TP_019, REQ_TP_020, REQ_TP_021, REQ_TP_022
 * @implements REQ_TP_001_E01, REQ_TP_001_E02, REQ_TP_001_E03
 * @implements REQ_TP_013_E01, REQ_TP_015_E01
 * @satisfies feat_req_someiptp_402
 * @satisfies feat_req_someiptp_403
 * @satisfies feat_req_someiptp_404
 */
TpSegmenter::TpSegmenter(const TpConfig& config)
    : config_(config) {
}

TpSegmenter::~TpSegmenter() = default;

/**
 * @brief Segment a message into TP segments
 * @implements REQ_TP_001, REQ_TP_002, REQ_TP_003, REQ_TP_004
 * @implements REQ_TP_001_E01
 */
/**
 * @brief Segment a message into TP segments
 * @implements REQ_TP_001, REQ_TP_002, REQ_TP_003, REQ_TP_004
 * @implements REQ_TP_001_E01
 */
TpResult TpSegmenter::segment_message(const Message& message, std::vector<TpSegment>& segments) {
    // Get the message payload (without headers - TP handles payload only)
    const std::vector<uint8_t>& payload = message.get_payload();

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
        std::vector<uint8_t> message_data = tp_message.serialize();

        TpSegment segment;
        segment.header.message_type = TpMessageType::SINGLE_MESSAGE;
        segment.header.message_length = static_cast<uint32_t>(payload.size());
        segment.header.segment_offset = 0;
        segment.header.segment_length = static_cast<uint16_t>(message_data.size());
        segment.header.sequence_number = next_sequence_number_++;
        segment.payload = std::move(message_data);  // Full message for single segment

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
 */
TpResult TpSegmenter::create_multi_segments(const Message& message,
                                          const std::vector<uint8_t>& payload,
                                          std::vector<TpSegment>& segments) {

    uint32_t total_length = static_cast<uint32_t>(payload.size());
    uint16_t payload_offset = 0;  // Offset into the payload data
    uint8_t sequence_number = next_sequence_number_++;

    // Create a copy of the message with TP flag added to message type
    Message tp_message = message;
    tp_message.set_message_type(add_tp_flag(message.get_message_type()));

    // First segment: header + first part of payload
    {
        TpSegment segment;
        segment.header.message_type = TpMessageType::FIRST_SEGMENT;
        segment.header.message_length = total_length;
        segment.header.segment_offset = 0;  // Always 0 for first segment
        segment.header.sequence_number = sequence_number;

        std::vector<uint8_t> header = tp_message.serialize();
        header.resize(16);  // Keep only header (16 bytes)

        // Add first part of payload
        size_t first_payload_size = std::min(static_cast<size_t>(config_.max_segment_size - 16),
                                           static_cast<size_t>(total_length));
        header.insert(header.end(), payload.begin(), payload.begin() + first_payload_size);

        segment.header.segment_length = static_cast<uint16_t>(header.size());
        segment.payload = std::move(header);

        segments.push_back(std::move(segment));
        payload_offset = first_payload_size;
    }

    // Subsequent segments
    while (payload_offset < total_length) {
        TpSegment segment;

        // Determine segment type
        uint32_t remaining_bytes = total_length - payload_offset;
        if (remaining_bytes <= config_.max_segment_size) {
            segment.header.message_type = TpMessageType::LAST_SEGMENT;
        } else {
            segment.header.message_type = TpMessageType::CONSECUTIVE_SEGMENT;
        }

        segment.header.message_length = total_length;
        segment.header.segment_offset = payload_offset;
        segment.header.sequence_number = sequence_number;

        // Calculate payload size for this segment
        uint16_t payload_size = static_cast<uint16_t>(
            std::min(static_cast<uint32_t>(config_.max_segment_size), remaining_bytes));


        segment.header.segment_length = payload_size;
        segment.payload.assign(payload.begin() + payload_offset,
                              payload.begin() + payload_offset + payload_size);

        segments.push_back(std::move(segment));
        payload_offset += payload_size;
    }

    next_sequence_number_ = (next_sequence_number_ + 1) % 256;

    return TpResult::SUCCESS;
}

void TpSegmenter::update_config(const TpConfig& config) {
    config_ = config;
}

/**
 * @brief Add TP flag to message type
 * @implements REQ_TP_007, REQ_TP_008
 */
MessageType TpSegmenter::add_tp_flag(MessageType type) const {
    // TP flag is bit 5 (0x20)
    uint8_t tp_flag = 0x20;
    return static_cast<MessageType>(static_cast<uint8_t>(type) | tp_flag);
}

} // namespace tp
} // namespace someip
