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

#ifndef SOMEIP_TP_TYPES_H
#define SOMEIP_TP_TYPES_H

#include <cstdint>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace someip {
namespace tp {

/**
 * @brief TP (Transport Protocol) result codes
 */
enum class TpResult : uint8_t {
    SUCCESS,
    MESSAGE_TOO_LARGE,
    SEGMENTATION_FAILED,
    REASSEMBLY_TIMEOUT,
    INVALID_SEGMENT,
    SEQUENCE_ERROR,
    NETWORK_ERROR,
    RESOURCE_EXHAUSTED,
    TIMEOUT
};

/**
 * @brief TP message types
 */
enum class TpMessageType : uint8_t {
    FIRST_SEGMENT = 0x20,    // First segment of a multi-segment message
    CONSECUTIVE_SEGMENT = 0x21, // Consecutive segments
    LAST_SEGMENT = 0x22,    // Last segment of a multi-segment message
    SINGLE_MESSAGE = 0x23   // Single message that fits in one segment
};

/**
 * @brief TP segmentation configuration
 */
struct TpConfig {
    uint32_t max_segment_size{1392};        // Maximum segment payload size (bytes) - 87*16 for alignment
    uint32_t max_message_size{1000000};     // Maximum total message size (1MB default)
    uint8_t max_retries{3};                 // Maximum retransmission attempts
    std::chrono::milliseconds retry_timeout{500};      // Timeout between retries
    std::chrono::milliseconds reassembly_timeout{5000}; // Timeout for reassembly
    uint32_t max_concurrent_transfers{10}; // Maximum concurrent transfers
    bool enable_acknowledgments{true};     // Enable acknowledgment mechanism
};

/**
 * @brief TP segment header
 */
struct TpSegmentHeader {
    uint32_t message_length{0};     // Total message length
    uint16_t segment_offset{0};     // Offset of this segment in the message
    uint16_t segment_length{0};     // Length of this segment's payload
    uint8_t sequence_number{0};     // Sequence number for ordering
    TpMessageType message_type{TpMessageType::SINGLE_MESSAGE};  // Type of TP message

    TpSegmentHeader() = default;
};

/**
 * @brief TP segment information
 */
struct TpSegment {
    TpSegmentHeader header;
    std::vector<uint8_t> payload;
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
    uint32_t retransmit_count{0};

    TpSegment() = default;
};

/**
 * @brief TP message being reassembled
 */
struct TpReassemblyBuffer {
    uint32_t message_id{0};                    // SOME/IP message ID
    uint32_t total_length{0};                  // Total expected message length
    std::vector<uint8_t> received_data;     // Buffer for received data
    std::vector<bool> received_segments;    // Track which segments received
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    uint8_t last_sequence_number{0};
    bool complete{false};

    TpReassemblyBuffer(uint32_t msg_id, uint32_t length)
        : message_id(msg_id), total_length(length) {
        received_data.resize(length);
        start_time = std::chrono::steady_clock::now();
    }

    bool is_segment_received(uint16_t offset, uint16_t length) const;
    void mark_segment_received(uint16_t offset, uint16_t length);
    bool is_complete() const;
    std::vector<uint8_t> get_complete_message() const;
};

/**
 * @brief TP transfer state
 */
enum class TpTransferState : uint8_t {
    IDLE,
    SEGMENTING,      // Sender: breaking message into segments
    SENDING,         // Sender: sending segments
    WAITING_ACK,     // Sender: waiting for acknowledgments
    RECEIVING,       // Receiver: receiving segments
    REASSEMBLING,    // Receiver: reassembling message
    COMPLETE,        // Transfer completed successfully
    FAILED,          // Transfer failed
    TIMEOUT          // Transfer timed out
};

/**
 * @brief TP transfer information
 */
struct TpTransfer {
    uint32_t transfer_id{0};
    uint32_t message_id{0};
    TpTransferState state{TpTransferState::IDLE};
    std::vector<TpSegment> segments;
    size_t next_segment_to_send{0};
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point last_activity{std::chrono::steady_clock::now()};
    uint32_t retry_count{0};

    TpTransfer() = default;

    TpTransfer(uint32_t id, uint32_t msg_id)
        : transfer_id(id), message_id(msg_id) {
        start_time = std::chrono::steady_clock::now();
        last_activity = start_time;
    }
};

/**
 * @brief TP callback types
 */
using TpCompletionCallback = std::function<void(uint32_t transfer_id, TpResult result)>;
using TpProgressCallback = std::function<void(uint32_t transfer_id, uint32_t bytes_transferred, uint32_t total_bytes)>;
using TpMessageCallback = std::function<void(uint32_t message_id, const std::vector<uint8_t>& data)>;

/**
 * @brief TP statistics
 */
struct TpStatistics {
    uint32_t messages_segmented{0};
    uint32_t messages_reassembled{0};
    uint32_t segments_sent{0};
    uint32_t segments_received{0};
    uint32_t retransmissions{0};
    uint32_t timeouts{0};
    uint32_t errors{0};
};

} // namespace tp
} // namespace someip

#endif // SOMEIP_TP_TYPES_H
