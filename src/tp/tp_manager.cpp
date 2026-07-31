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

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/thread.h"
#include "someip/message.h"
#include "tp/tp_reassembler.h"
#include "tp/tp_segmenter.h"
#include "tp/tp_types.h"

#include <chrono>
#include <cstdint>
#include <utility>

namespace someip::tp {

/**
 * @brief SOME/IP-TP Manager implementation
 * @satisfies feat_req_someiptp_400
 * @satisfies feat_req_someiptp_401
 */
TpManager::TpManager(const TpConfig& config)
    : config_(config),
      segmenter_(std::in_place, config),
      reassembler_(std::in_place, config) {
}

TpManager::~TpManager() = default;

bool TpManager::initialize() {
    // Initialization if needed
    return true;
}

void TpManager::shutdown() {
    platform::ScopedLock const lock(transfers_mutex_);
    active_transfers_.clear();
}

bool TpManager::needs_segmentation(const Message& message) const {
    platform::ByteBuffer const data = message.serialize();
    return data.size() > config_.max_segment_size;
}

/**
 * @brief Segment a message for TP transmission
 * @implements REQ_TP_050, REQ_TP_051
 * @implements REQ_TP_050_E01
 */
TpResult TpManager::segment_message(const Message& message, uint32_t& transfer_id) {
    platform::ScopedLock const lock(transfers_mutex_);

    // Check if we have capacity for new transfers
    if (active_transfers_.size() >= config_.max_concurrent_transfers) {
        return TpResult::RESOURCE_EXHAUSTED;
    }

    // Create new transfer
    transfer_id = next_transfer_id_++;
    uint32_t const message_id =
        (static_cast<uint32_t>(message.get_service_id()) << 16U) |
        static_cast<uint32_t>(message.get_method_id());

    TpTransfer transfer(transfer_id, message_id);

    TpSegmentVector segments;
    if (!segmenter_) {
        return TpResult::RESOURCE_EXHAUSTED;
    }
    TpResult const result = segmenter_->segment_message(message, segments);

    if (result != TpResult::SUCCESS) {
        return result;
    }

    transfer.segments = std::move(segments);
    transfer.state = TpTransferState::SENDING;

    if (active_transfers_.size() >= active_transfers_.max_size()) {
        return TpResult::RESOURCE_EXHAUSTED;
    }
    active_transfers_[transfer_id] = std::move(transfer);
    statistics_.messages_segmented++;

    return TpResult::SUCCESS;
}

/**
 * @brief Get next segment to send
 * @implements REQ_TP_052, REQ_TP_053, REQ_TP_054
 */
TpResult TpManager::get_next_segment(uint32_t transfer_id, TpSegment& segment) {
    platform::ScopedLock const lock(transfers_mutex_);

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

/**
 * @brief Handle a received TP segment
 * @implements REQ_TP_055, REQ_TP_056, REQ_TP_057
 * @implements REQ_TP_050_E02
 */
bool TpManager::handle_received_segment(const TpSegment& segment, platform::ByteBuffer& complete_message) {
    // Update statistics
    statistics_.segments_received++;

    if (segment.header.message_type == TpMessageType::SINGLE_MESSAGE) {
        if (segment.header.segment_length != segment.payload.size()) {
            return false;
        }
        if (segment.header.message_length > config_.max_message_size) {
            return false;
        }
        if (segment.payload.empty()) {
            return false;
        }
        complete_message = segment.payload;
        return true;
    }

    if (!reassembler_) {
        return false;
    }
    return reassembler_->process_segment(segment, complete_message);
}

TpResult TpManager::acknowledge_segments(uint32_t transfer_id, const platform::Vector<uint16_t>& /*segments_acknowledged*/) {
    platform::ScopedLock const lock(transfers_mutex_);

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
    platform::ScopedLock const lock(transfers_mutex_);

    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        return TpResult::INVALID_SEGMENT;
    }

    it->second.state = TpTransferState::FAILED;
    active_transfers_.erase(it);

    return TpResult::SUCCESS;
}

TpTransferState TpManager::get_transfer_status(uint32_t transfer_id) const {
    platform::ScopedLock const lock(transfers_mutex_);

    auto it = active_transfers_.find(transfer_id);
    if (it == active_transfers_.end()) {
        return TpTransferState::FAILED;
    }

    return it->second.state;
}

void TpManager::set_completion_callback(TpCompletionCallback callback) {
    platform::ScopedLock const lock(transfers_mutex_);
    completion_callback_ = std::move(callback);
}

void TpManager::set_progress_callback(TpProgressCallback callback) {
    platform::ScopedLock const lock(transfers_mutex_);
    progress_callback_ = std::move(callback);
}

void TpManager::set_message_callback(TpMessageCallback callback) {
    platform::ScopedLock const lock(transfers_mutex_);
    message_callback_ = std::move(callback);
}

void TpManager::process_timeouts() {
    platform::Vector<std::pair<uint32_t, TpResult>> timed_out;
    TpCompletionCallback cb;

    {
        platform::ScopedLock const lock(transfers_mutex_);

        cb = completion_callback_;

        auto const now = std::chrono::steady_clock::now();

        for (auto it = active_transfers_.begin(); it != active_transfers_.end(); ) {
            TpTransfer& transfer = it->second;
            auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - transfer.last_activity);

            if (elapsed > config_.reassembly_timeout) {
                transfer.state = TpTransferState::TIMEOUT;
                statistics_.timeouts++;
                timed_out.emplace_back(transfer.transfer_id, TpResult::TIMEOUT);
                it = active_transfers_.erase(it);
            } else {
                ++it;
            }
        }

        if (reassembler_) {
            reassembler_->process_timeouts();
        }
        cleanup_completed_transfers();
    }

    if (cb) {
        for (const auto& [id, result] : timed_out) {
            cb(id, result);
        }
    }
}

/**
 * @brief Get TP statistics
 * @implements REQ_TP_060, REQ_TP_061, REQ_TP_062, REQ_TP_063
 */
TpStatistics TpManager::get_statistics() const {
    return statistics_;
}

/**
 * @brief Update TP manager configuration
 * @implements REQ_TP_070, REQ_TP_071, REQ_TP_072, REQ_TP_073, REQ_TP_074, REQ_TP_075
 * @implements REQ_TP_076, REQ_TP_077, REQ_TP_078
 */
void TpManager::update_config(const TpConfig& config) {
    config_ = config;
    if (segmenter_) {
        segmenter_->update_config(config);
    }
    if (reassembler_) {
        reassembler_->update_config(config);
    }
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

}  // namespace someip::tp
