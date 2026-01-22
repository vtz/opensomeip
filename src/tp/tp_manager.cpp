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

#include "tp/tp_manager.h"
#include "tp/tp_segmenter.h"
#include "tp/tp_reassembler.h"
#include "someip/message.h"
#include <algorithm>

namespace someip {
namespace tp {

/**
 * @brief SOME/IP-TP Manager implementation
 * @implements REQ_ARCH_001
 * @implements REQ_TP_050, REQ_TP_051, REQ_TP_052, REQ_TP_053, REQ_TP_054, REQ_TP_055, REQ_TP_056, REQ_TP_057
 * @implements REQ_TP_060, REQ_TP_061, REQ_TP_062, REQ_TP_063
 * @implements REQ_TP_050_E01, REQ_TP_050_E02
 * @satisfies feat_req_someiptp_400
 * @satisfies feat_req_someiptp_401
 */
TpManager::TpManager(const TpConfig& config)
    : config_(config),
      segmenter_(std::make_unique<TpSegmenter>(config)),
      reassembler_(std::make_unique<TpReassembler>(config)) {
}

TpManager::~TpManager() = default;

bool TpManager::initialize() {
    // Initialization if needed
    return true;
}

void TpManager::shutdown() {
    std::scoped_lock lock(transfers_mutex_);
    active_transfers_.clear();
}

bool TpManager::needs_segmentation(const Message& message) const {
    std::vector<uint8_t> data = message.serialize();
    return data.size() > config_.max_segment_size;
}

TpResult TpManager::segment_message(const Message& message, uint32_t& transfer_id) {
    std::scoped_lock lock(transfers_mutex_);

    // Check if we have capacity for new transfers
    if (active_transfers_.size() >= config_.max_concurrent_transfers) {
        return TpResult::RESOURCE_EXHAUSTED;
    }

    // Create new transfer
    transfer_id = next_transfer_id_++;
    uint32_t message_id = (static_cast<uint32_t>(message.get_service_id()) << 16) |
                         message.get_method_id();

    TpTransfer transfer(transfer_id, message_id);

    // Segment the message
    std::vector<TpSegment> segments;
    TpResult result = segmenter_->segment_message(message, segments);

    if (result != TpResult::SUCCESS) {
        return result;
    }

    transfer.segments = std::move(segments);
    transfer.state = TpTransferState::SENDING;

    active_transfers_[transfer_id] = std::move(transfer);
    statistics_.messages_segmented++;

    return TpResult::SUCCESS;
}

TpResult TpManager::get_next_segment(uint32_t transfer_id, TpSegment& segment) {
    std::scoped_lock lock(transfers_mutex_);

    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        return TpResult::INVALID_SEGMENT;
    }

    TpTransfer& transfer = it->second;

    if (transfer.next_segment_to_send >= transfer.segments.size()) {
        transfer.state = TpTransferState::COMPLETE;
        segment = TpSegment();  // Clear the segment
        return TpResult::SUCCESS;  // No more segments
    }

    segment = transfer.segments[transfer.next_segment_to_send];
    transfer.next_segment_to_send++;
    transfer.last_activity = std::chrono::steady_clock::now();

    statistics_.segments_sent++;

    return TpResult::SUCCESS;
}

bool TpManager::handle_received_segment(const TpSegment& segment, std::vector<uint8_t>& complete_message) {
    // Update statistics
    statistics_.segments_received++;

    // Check if this is a single-message segment
    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        complete_message = segment.payload;
        return true;
    }

    // Handle multi-segment message
    return reassembler_->process_segment(segment, complete_message);
}

TpResult TpManager::acknowledge_segments(uint32_t transfer_id, const std::vector<uint16_t>& segments_acknowledged) {
    std::scoped_lock lock(transfers_mutex_);

    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        return TpResult::INVALID_SEGMENT;
    }

    // For now, we assume all segments are acknowledged
    // In a full implementation, we'd track individual segment acknowledgments
    it->second.last_activity = std::chrono::steady_clock::now();

    return TpResult::SUCCESS;
}

TpResult TpManager::cancel_transfer(uint32_t transfer_id) {
    std::scoped_lock lock(transfers_mutex_);

    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        return TpResult::INVALID_SEGMENT;
    }

    it->second.state = TpTransferState::FAILED;
    active_transfers_.erase(it);

    return TpResult::SUCCESS;
}

TpTransferState TpManager::get_transfer_status(uint32_t transfer_id) const {
    std::scoped_lock lock(transfers_mutex_);

    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        return TpTransferState::FAILED;
    }

    return it->second.state;
}

void TpManager::set_completion_callback(TpCompletionCallback callback) {
    completion_callback_ = std::move(callback);
}

void TpManager::set_progress_callback(TpProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

void TpManager::set_message_callback(TpMessageCallback callback) {
    message_callback_ = std::move(callback);
}

void TpManager::process_timeouts() {
    std::scoped_lock lock(transfers_mutex_);

    auto now = std::chrono::steady_clock::now();

    for (auto it = active_transfers_.begin(); it != active_transfers_.end(); ) {
        TpTransfer& transfer = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - transfer.last_activity);

        if (elapsed > config_.reassembly_timeout) {
            transfer.state = TpTransferState::TIMEOUT;
            statistics_.timeouts++;

            if (completion_callback_) {
                completion_callback_(transfer.transfer_id, TpResult::TIMEOUT);
            }

            it = active_transfers_.erase(it);
        } else {
            ++it;
        }
    }

    // Process reassembler timeouts
    reassembler_->process_timeouts();

    // Cleanup completed transfers
    cleanup_completed_transfers();
}

TpStatistics TpManager::get_statistics() const {
    return statistics_;
}

void TpManager::update_config(const TpConfig& config) {
    config_ = config;
    segmenter_->update_config(config);
    reassembler_->update_config(config);
}

void TpManager::cleanup_completed_transfers() {
    for (auto it = active_transfers_.begin(); it != active_transfers_.end(); ) {
        if (it->second.state == TpTransferState::COMPLETE ||
            it->second.state == TpTransferState::FAILED) {
            it = active_transfers_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace tp
} // namespace someip
