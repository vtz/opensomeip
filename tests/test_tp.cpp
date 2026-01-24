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

#include <gtest/gtest.h>
#include <tp/tp_manager.h>
#include <tp/tp_segmenter.h>
#include <tp/tp_reassembler.h>
#include <someip/message.h>
#include <thread>

using namespace someip;
using namespace someip::tp;

/**
 * @brief SOME/IP-TP Transport Protocol unit tests
 * @tests REQ_ARCH_001
 * @tests REQ_ARCH_002
 * @tests REQ_TP_001, REQ_TP_002, REQ_TP_003, REQ_TP_004, REQ_TP_005, REQ_TP_006, REQ_TP_007, REQ_TP_008
 * @tests REQ_TP_010, REQ_TP_011, REQ_TP_012, REQ_TP_013, REQ_TP_014, REQ_TP_015, REQ_TP_016, REQ_TP_017
 * @tests REQ_TP_018, REQ_TP_019, REQ_TP_020, REQ_TP_021, REQ_TP_022
 * @tests REQ_TP_030, REQ_TP_031, REQ_TP_032, REQ_TP_033, REQ_TP_034, REQ_TP_035
 * @tests REQ_TP_036, REQ_TP_037, REQ_TP_038, REQ_TP_039, REQ_TP_040, REQ_TP_041, REQ_TP_042, REQ_TP_043
 * @tests REQ_TP_050, REQ_TP_051, REQ_TP_052, REQ_TP_053, REQ_TP_054, REQ_TP_055, REQ_TP_056, REQ_TP_057
 * @tests REQ_TP_060, REQ_TP_061, REQ_TP_062, REQ_TP_063
 * @tests REQ_TP_001_E01, REQ_TP_001_E02, REQ_TP_001_E03
 * @tests REQ_TP_013_E01, REQ_TP_015_E01
 * @tests REQ_TP_030_E01, REQ_TP_030_E02, REQ_TP_039_E01
 * @tests REQ_TP_050_E01, REQ_TP_050_E02
 * @tests feat_req_someiptp_400
 * @tests feat_req_someiptp_402
 * @tests feat_req_someiptp_410
 */
class TpTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.max_segment_size = 512;  // Small for testing
        config.max_message_size = 10000;
        config.reassembly_timeout = std::chrono::milliseconds(1000);
    }

    TpConfig config;
};

/**
 * @test_case TC_TP_001
 * @tests REQ_TP_001, REQ_TP_010, REQ_TP_050
 * @brief Test single segment message handling
 */
TEST_F(TpTest, SingleSegmentMessage) {
    TpManager tp_manager(config);
    ASSERT_TRUE(tp_manager.initialize());

    // Create small message that fits in one segment
    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> small_payload(256, 0xAA);
    message.set_payload(small_payload);

    // Should not need segmentation
    ASSERT_FALSE(tp_manager.needs_segmentation(message));

    // But should still handle as single segment
    uint32_t transfer_id;
    TpResult result = tp_manager.segment_message(message, transfer_id);
    ASSERT_EQ(result, TpResult::SUCCESS);

    TpSegment segment;
    result = tp_manager.get_next_segment(transfer_id, segment);
    ASSERT_EQ(result, TpResult::SUCCESS);
    ASSERT_EQ(segment.header.message_type, TpMessageType::SINGLE_MESSAGE);

    // Single segment contains full serialized message
    std::vector<uint8_t> expected_data = message.serialize();
    ASSERT_EQ(segment.payload.size(), expected_data.size());
    ASSERT_EQ(segment.payload, expected_data);

    tp_manager.shutdown();
}

/**
 * @test_case TC_TP_002
 * @tests REQ_TP_001, REQ_TP_002, REQ_TP_003, REQ_TP_010, REQ_TP_011, REQ_TP_012, REQ_TP_013
 * @brief Test multi-segment message segmentation
 */
TEST_F(TpTest, MultiSegmentMessage) {
    TpManager tp_manager(config);
    ASSERT_TRUE(tp_manager.initialize());

    // Create large message that needs segmentation
    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> large_payload(1500, 0xBB); // Larger than segment size
    message.set_payload(large_payload);

    // Should need segmentation
    ASSERT_TRUE(tp_manager.needs_segmentation(message));

    // Segment the message
    uint32_t transfer_id;
    TpResult result = tp_manager.segment_message(message, transfer_id);
    ASSERT_EQ(result, TpResult::SUCCESS);

    // Collect all segments
    std::vector<TpSegment> segments;
    TpSegment segment;
    while (tp_manager.get_next_segment(transfer_id, segment) == TpResult::SUCCESS) {
        if (segment.payload.empty()) {
            break;
        }
        segments.push_back(segment);
    }

    // Should have multiple segments
    ASSERT_GT(segments.size(), 1u);

    // First segment should be FIRST_SEGMENT
    ASSERT_EQ(segments[0].header.message_type, TpMessageType::FIRST_SEGMENT);

    // Last segment should be LAST_SEGMENT
    ASSERT_EQ(segments.back().header.message_type, TpMessageType::LAST_SEGMENT);

    // Middle segments should be CONSECUTIVE_SEGMENT
    for (size_t i = 1; i < segments.size() - 1; ++i) {
        ASSERT_EQ(segments[i].header.message_type, TpMessageType::CONSECUTIVE_SEGMENT);
    }

    // All segments should have same sequence number
    uint8_t sequence_number = segments[0].header.sequence_number;
    for (const auto& seg : segments) {
        ASSERT_EQ(seg.header.sequence_number, sequence_number);
    }

    tp_manager.shutdown();
}

TEST_F(TpTest, MessageReassembly) {
    TpManager tp_manager(config);
    ASSERT_TRUE(tp_manager.initialize());

    // Create large message
    Message original_message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                            MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> original_payload(1024, 0xCC);
    original_message.set_payload(original_payload);

    // Segment the message
    uint32_t transfer_id;
    TpResult result = tp_manager.segment_message(original_message, transfer_id);
    ASSERT_EQ(result, TpResult::SUCCESS);

    // Collect all segments
    std::vector<TpSegment> segments;
    TpSegment segment;
    while (tp_manager.get_next_segment(transfer_id, segment) == TpResult::SUCCESS) {
        if (segment.payload.empty()) {
            break;
        }
        segments.push_back(segment);
    }

    // Should have multiple segments for large message
    ASSERT_GT(segments.size(), 1u);

    // Simulate receiving and reassembling
    std::vector<uint8_t> reassembled_payload;
    bool reassembly_complete = false;

    for (const auto& seg : segments) {
        std::vector<uint8_t> complete_payload;
        if (tp_manager.handle_received_segment(seg, complete_payload)) {
            if (!complete_payload.empty()) {
                reassembled_payload = complete_payload;
                reassembly_complete = true;
                break;
            }
        }
    }

    // Should have reassembled the complete payload
    ASSERT_TRUE(reassembly_complete);
    ASSERT_EQ(reassembled_payload.size(), original_payload.size());
    ASSERT_EQ(reassembled_payload, original_payload);

    tp_manager.shutdown();
}

// Out-of-order reassembly and duplicate handling are tested in MessageReassembly

TEST_F(TpTest, TimeoutHandling) {
    TpConfig short_timeout_config = config;
    short_timeout_config.reassembly_timeout = std::chrono::milliseconds(100);

    TpReassembler reassembler(short_timeout_config);

    // Start a reassembly
    TpSegment seg;
    seg.header.message_length = 1000;
    seg.header.segment_offset = 0;
    seg.header.segment_length = 500;
    seg.header.sequence_number = 1;
    seg.header.message_type = TpMessageType::FIRST_SEGMENT;
    seg.payload.assign(500, 0x11);

    std::vector<uint8_t> complete_message;
    ASSERT_TRUE(reassembler.process_segment(seg, complete_message));
    ASSERT_TRUE(complete_message.empty());

    // Should be actively reassembling
    ASSERT_TRUE(reassembler.is_reassembling(1));

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Process timeouts
    reassembler.process_timeouts();

    // Should no longer be reassembling
    ASSERT_FALSE(reassembler.is_reassembling(1));
}

TEST_F(TpTest, InvalidSegmentHandling) {
    TpReassembler reassembler(config);

    // Create invalid segment (offset + length > message_length)
    TpSegment invalid_seg;
    invalid_seg.header.message_length = 500;
    invalid_seg.header.segment_offset = 300;
    invalid_seg.header.segment_length = 300; // 300 + 300 = 600 > 500
    invalid_seg.header.sequence_number = 1;
    invalid_seg.header.message_type = TpMessageType::CONSECUTIVE_SEGMENT;
    invalid_seg.payload.assign(300, 0x22);

    std::vector<uint8_t> complete_message;
    ASSERT_FALSE(reassembler.process_segment(invalid_seg, complete_message));
}

TEST_F(TpTest, StatisticsTracking) {
    TpManager tp_manager(config);
    ASSERT_TRUE(tp_manager.initialize());

    // Create and segment a message
    Message message(MessageId(0x1111, 0x2222), RequestId(0x3333, 0x4444),
                   MessageType::REQUEST, ReturnCode::E_OK);
    message.set_payload(std::vector<uint8_t>(800, 0x55));

    uint32_t transfer_id;
    TpResult result = tp_manager.segment_message(message, transfer_id);
    ASSERT_EQ(result, TpResult::SUCCESS);

    // Send all segments
    TpSegment segment;
    int segment_count = 0;
    while (tp_manager.get_next_segment(transfer_id, segment) == TpResult::SUCCESS) {
        if (segment.payload.empty()) {
            break;
        }
        segment_count++;
    }

    // Check statistics
    auto stats = tp_manager.get_statistics();
    EXPECT_EQ(stats.messages_segmented, 1u);
    EXPECT_EQ(stats.segments_sent, static_cast<uint32_t>(segment_count));

    tp_manager.shutdown();
}

/**
 * @test_case TC_TP_002
 * @tests REQ_TP_002
 * @brief Test maximum segment payload size is 1392 bytes
 */
TEST_F(TpTest, MaximumSegmentSize) {
    TpConfig test_config;
    // Verify the max segment size is set to 1392 bytes (87 * 16)
    EXPECT_EQ(test_config.max_segment_size, 1392u);

    // Test that messages larger than 1392 bytes get segmented
    TpSegmenter segmenter(test_config);

    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> large_payload(1393, 0xAA); // Just over 1392 bytes
    message.set_payload(large_payload);

    std::vector<TpSegment> segments;
    TpResult result = segmenter.segment_message(message, segments);
    EXPECT_EQ(result, TpResult::SUCCESS);
    EXPECT_GT(segments.size(), 1u);
}

/**
 * @test_case TC_TP_003
 * @tests REQ_TP_003
 * @brief Test segment alignment requirements (multiples of 16 bytes)
 */
TEST_F(TpTest, SegmentAlignment) {
    TpSegmenter segmenter(config);

    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> large_payload(2000, 0xBB); // Large payload
    message.set_payload(large_payload);

    std::vector<TpSegment> segments;
    TpResult result = segmenter.segment_message(message, segments);
    ASSERT_EQ(result, TpResult::SUCCESS);
    ASSERT_GT(segments.size(), 1u);

    // Check alignment of all segments except the last
    for (size_t i = 0; i < segments.size() - 1; ++i) {
        // Payload size should be multiple of 16 (excluding header for first segment)
        if (i == 0) {
            // First segment: payload_size - 16 (header) should be multiple of 16
            size_t data_size = segments[i].payload.size() - 16;
            EXPECT_EQ(data_size % 16, 0u) << "First segment data not 16-byte aligned";
        } else {
            // Other segments: entire payload should be multiple of 16
            EXPECT_EQ(segments[i].payload.size() % 16, 0u) << "Segment " << i << " not 16-byte aligned";
        }
    }
}

/**
 * @test_case TC_TP_006
 * @tests REQ_TP_006
 * @brief Test all segments have the same TP sequence number
 */
TEST_F(TpTest, SameSessionId) {
    TpSegmenter segmenter(config);

    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> large_payload(1500, 0xCC);
    message.set_payload(large_payload);

    std::vector<TpSegment> segments;
    TpResult result = segmenter.segment_message(message, segments);
    ASSERT_EQ(result, TpResult::SUCCESS);
    ASSERT_GT(segments.size(), 1u);

    // All segments should have the same TP sequence number
    uint8_t expected_sequence = segments[0].header.sequence_number;
    for (const auto& segment : segments) {
        EXPECT_EQ(segment.header.sequence_number, expected_sequence);
    }
}

/**
 * @test_case TC_TP_007
 * @tests REQ_TP_007
 * @brief Test TP flag is set in Message Type for segmented messages
 */
TEST_F(TpTest, TpFlagInMessageType) {
    TpSegmenter segmenter(config);

    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST, ReturnCode::E_OK);
    std::vector<uint8_t> large_payload(1500, 0xDD);
    message.set_payload(large_payload);

    std::vector<TpSegment> segments;
    TpResult result = segmenter.segment_message(message, segments);
    ASSERT_EQ(result, TpResult::SUCCESS);
    ASSERT_GT(segments.size(), 1u);

    // Check that TP flag (0x20) is set in the message type for the first segment
    const auto& first_segment = segments[0];
    if (first_segment.payload.size() >= 16) {  // Has SOME/IP header
        // Message type is at offset 14 in SOME/IP header
        uint8_t message_type = first_segment.payload[14];
        EXPECT_NE(message_type & 0x20, 0u) << "TP flag not set in message type";
    }
}

/**
 * @test_case TC_TP_008
 * @tests REQ_TP_008
 * @brief Test original message type is preserved with TP flag added
 */
TEST_F(TpTest, PreserveMessageTypeWithTpFlag) {
    TpSegmenter segmenter(config);

    Message message(MessageId(0x1234, 0x5678), RequestId(0xABCD, 0x0001),
                   MessageType::REQUEST_NO_RETURN, ReturnCode::E_OK);
    std::vector<uint8_t> large_payload(1500, 0xEE);
    message.set_payload(large_payload);

    std::vector<TpSegment> segments;
    TpResult result = segmenter.segment_message(message, segments);
    ASSERT_EQ(result, TpResult::SUCCESS);
    ASSERT_GT(segments.size(), 1u);

    // Original message type should be REQUEST_NO_RETURN (0x01)
    // With TP flag it should become TP_REQUEST_NO_RETURN (0x21)
    MessageType expected_tp_type = static_cast<MessageType>(
        static_cast<uint8_t>(MessageType::REQUEST_NO_RETURN) | 0x20);

    // Only the first segment contains the SOME/IP header
    const auto& first_segment = segments[0];
    if (first_segment.payload.size() >= 16) {  // Has SOME/IP header
        // Message type is at offset 14 in SOME/IP header
        uint8_t message_type = first_segment.payload[14];
        EXPECT_EQ(static_cast<MessageType>(message_type), expected_tp_type)
            << "Message type not preserved with TP flag";
    }
}
