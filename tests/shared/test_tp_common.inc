/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared SOME/IP-TP segmentation/reassembly tests for RTOS and Zephyr suites.
 *
 * Platform-independent subset of the host test_tp.cpp suite covering:
 *   - Single-segment message handling
 *   - Multi-segment segmentation
 *   - Message reassembly roundtrip
 *   - Segment alignment (16-byte)
 *   - Invalid segment rejection
 *   - Message-too-large error
 *   - Statistics tracking
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 */

#ifndef SOMEIP_TEST_TP_COMMON_INC
#define SOMEIP_TEST_TP_COMMON_INC

#include "tp/tp_manager.h"
#include "tp/tp_segmenter.h"
#include "tp/tp_reassembler.h"
#include "tp/tp_types.h"
#include "someip/message.h"
#include "platform/buffer_pool.h"
#include <cstdio>

static void test_tp() {
    using namespace someip;
    using namespace someip::tp;

    printf("\n--- SOME/IP-TP tests ---\n");

    TpConfig config;
    config.max_segment_size = 512;
    config.max_message_size = 10000;
    config.reassembly_timeout = std::chrono::milliseconds(1000);

    // Single-segment message: small payload fits in one segment
    {
        TpManager manager(config);
        CHECK(manager.initialize(), "tp_manager_init");

        Message msg(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                    MessageType::REQUEST, ReturnCode::E_OK);
        platform::ByteBuffer small_payload(256, 0xAA);
        msg.set_payload(small_payload);

        CHECK(!manager.needs_segmentation(msg), "tp_small_no_segmentation");

        uint32_t transfer_id;
        TpResult result = manager.segment_message(msg, transfer_id);
        CHECK(result == TpResult::SUCCESS, "tp_single_segment_ok");

        TpSegment seg;
        result = manager.get_next_segment(transfer_id, seg);
        CHECK(result == TpResult::SUCCESS, "tp_get_single_segment");
        CHECK(seg.header.message_type == TpMessageType::SINGLE_MESSAGE,
              "tp_single_message_type");

        manager.shutdown();
    }

    // Multi-segment: large payload gets split
    {
        TpManager manager(config);
        CHECK(manager.initialize(), "tp_multi_manager_init");

        Message msg(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                    MessageType::REQUEST, ReturnCode::E_OK);
        platform::ByteBuffer large_payload(1500, 0xBB);
        msg.set_payload(large_payload);

        CHECK(manager.needs_segmentation(msg), "tp_large_needs_segmentation");

        uint32_t transfer_id;
        TpResult result = manager.segment_message(msg, transfer_id);
        CHECK(result == TpResult::SUCCESS, "tp_multi_segment_ok");

        TpSegmentVector segments;
        TpSegment seg;
        while (manager.get_next_segment(transfer_id, seg) == TpResult::SUCCESS) {
            if (seg.payload.empty()) break;
            segments.push_back(seg);
        }

        const bool has_multiple = segments.size() > 1;
        CHECK(has_multiple, "tp_multiple_segments");
        if (has_multiple) {
            CHECK(segments.front().header.message_type == TpMessageType::FIRST_SEGMENT,
                  "tp_first_segment_type");
            CHECK(segments.back().header.message_type == TpMessageType::LAST_SEGMENT,
                  "tp_last_segment_type");

            uint8_t seq = segments[0].header.sequence_number;
            bool seq_ok = true;
            for (const auto& s : segments) {
                if (s.header.sequence_number != seq) { seq_ok = false; break; }
            }
            CHECK(seq_ok, "tp_same_sequence_number");

            bool mid_ok = true;
            for (size_t i = 1; i + 1 < segments.size(); ++i) {
                if (segments[i].header.message_type != TpMessageType::CONSECUTIVE_SEGMENT) {
                    mid_ok = false; break;
                }
            }
            CHECK(mid_ok, "tp_middle_segments_consecutive");
        }

        manager.shutdown();
    }

    // Reassembly roundtrip: segment → reassemble → verify payload
    {
        TpManager manager(config);
        CHECK(manager.initialize(), "tp_reassembly_manager_init");

        Message original(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                         MessageType::REQUEST, ReturnCode::E_OK);
        platform::ByteBuffer original_payload(1024, 0xCC);
        original.set_payload(original_payload);

        uint32_t transfer_id;
        TpResult seg_result = manager.segment_message(original, transfer_id);
        CHECK(seg_result == TpResult::SUCCESS, "tp_reassembly_segment_ok");

        TpSegmentVector segments;
        TpSegment seg;
        while (manager.get_next_segment(transfer_id, seg) == TpResult::SUCCESS) {
            if (seg.payload.empty()) break;
            segments.push_back(seg);
        }

        CHECK(segments.size() > 1, "tp_reassembly_has_segments");

        platform::ByteBuffer reassembled;
        bool complete = false;
        for (const auto& s : segments) {
            platform::ByteBuffer payload;
            if (manager.handle_received_segment(s, payload)) {
                if (!payload.empty()) {
                    reassembled = payload;
                    complete = true;
                    break;
                }
            }
        }

        CHECK(complete, "tp_reassembly_complete");
        CHECK(reassembled.size() == original_payload.size(), "tp_reassembly_size");
        CHECK(reassembled == original_payload, "tp_reassembly_data");

        manager.shutdown();
    }

    // Segment alignment: non-last segments must be 16-byte aligned
    {
        TpSegmenter segmenter(config);

        Message msg(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                    MessageType::REQUEST, ReturnCode::E_OK);
        platform::ByteBuffer payload(2000, 0xDD);
        msg.set_payload(payload);

        TpSegmentVector segments;
        TpResult result = segmenter.segment_message(msg, segments);
        CHECK(result == TpResult::SUCCESS, "tp_alignment_segment_ok");

        const bool align_multi = segments.size() > 1;
        CHECK(align_multi, "tp_alignment_multi");
        if (align_multi) {
            bool aligned = true;
            bool uniform = true;
            const size_t first_sz = segments.front().payload.size();
            for (size_t i = 0; i + 1 < segments.size(); ++i) {
                size_t sz = segments[i].payload.size();
                if (sz < 20 || (sz - 20) % 16 != 0) { aligned = false; break; }
                if (sz != first_sz) { uniform = false; }
            }
            CHECK(aligned, "tp_segments_16byte_aligned");
            CHECK(uniform, "tp_nonfinal_segments_uniform_size");
        }
    }

    // Invalid segment: offset + length > message_length
    {
        TpReassembler reassembler(config);

        TpSegment invalid;
        invalid.header.message_length = 500;
        invalid.header.segment_offset = 300;
        invalid.header.segment_length = 300;
        invalid.header.sequence_number = 1;
        invalid.header.message_type = TpMessageType::CONSECUTIVE_SEGMENT;
        invalid.payload = platform::ByteBuffer(300, 0x22);

        platform::ByteBuffer out;
        CHECK(!reassembler.process_segment(invalid, out),
              "tp_invalid_segment_rejected");
    }

    // Message too large
    {
        TpConfig small_cfg;
        small_cfg.max_segment_size = 512;
        small_cfg.max_message_size = 1000;
        TpSegmenter segmenter(small_cfg);

        Message msg(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001));
        platform::ByteBuffer big(2000, 0xEE);
        msg.set_payload(big);

        TpSegmentVector segments;
        TpResult result = segmenter.segment_message(msg, segments);
        CHECK(result == TpResult::MESSAGE_TOO_LARGE, "tp_too_large_rejected");
        CHECK(segments.empty(), "tp_too_large_no_segments");
    }

    // Statistics tracking
    {
        TpManager manager(config);
        CHECK(manager.initialize(), "tp_stats_manager_init");

        Message msg(MessageId(0x1111, 0x2222), RequestId(0x3333, 0x4444),
                    MessageType::REQUEST, ReturnCode::E_OK);
        msg.set_payload(platform::ByteBuffer(800, 0x55));

        uint32_t transfer_id;
        TpResult seg_result = manager.segment_message(msg, transfer_id);
        CHECK(seg_result == TpResult::SUCCESS, "tp_stats_segment_ok");

        TpSegment seg;
        int count = 0;
        while (manager.get_next_segment(transfer_id, seg) == TpResult::SUCCESS) {
            if (seg.payload.empty()) break;
            count++;
        }

        auto stats = manager.get_statistics();
        CHECK(stats.messages_segmented == 1u, "tp_stats_messages_segmented");
        CHECK(stats.segments_sent == static_cast<uint32_t>(count),
              "tp_stats_segments_sent");

        manager.shutdown();
    }

    // Invalid transfer ID
    {
        TpManager manager(config);
        CHECK(manager.initialize(), "tp_invalid_xfer_manager_init");

        TpSegment seg;
        CHECK(manager.get_next_segment(99999, seg) == TpResult::INVALID_SEGMENT,
              "tp_invalid_transfer_id");

        manager.shutdown();
    }
}

#endif // SOMEIP_TEST_TP_COMMON_INC
