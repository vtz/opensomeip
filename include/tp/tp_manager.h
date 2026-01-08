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

#ifndef SOMEIP_TP_MANAGER_H
#define SOMEIP_TP_MANAGER_H

#include "tp_types.h"
#include "../someip/message.h"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace someip {
namespace tp {

/**
 * @brief Forward declarations
 */
class TpSegmenter;
class TpReassembler;

/**
 * @brief SOME/IP Transport Protocol Manager
 *
 * Handles segmentation of large messages and reassembly of received segments.
 * Integrates with the transport layer to provide transparent large message support.
 */
class TpManager {
public:
    /**
     * @brief Constructor
     * @param config TP configuration
     */
    explicit TpManager(const TpConfig& config = TpConfig());

    /**
     * @brief Destructor
     */
    ~TpManager();

    // Delete copy and move operations
    TpManager(const TpManager&) = delete;
    TpManager& operator=(const TpManager&) = delete;
    TpManager(TpManager&&) = delete;
    TpManager& operator=(TpManager&&) = delete;

    /**
     * @brief Initialize the TP manager
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Shutdown the TP manager
     */
    void shutdown();

    /**
     * @brief Check if a message needs TP segmentation
     *
     * @param message The message to check
     * @return true if message should use TP, false if it can be sent directly
     */
    bool needs_segmentation(const Message& message) const;

    /**
     * @brief Segment a large message for transmission
     *
     * @param message The message to segment
     * @param transfer_id Unique transfer identifier (output)
     * @return SUCCESS if segmentation started, error code otherwise
     */
    TpResult segment_message(const Message& message, uint32_t& transfer_id);

    /**
     * @brief Get next segment for transmission
     *
     * @param transfer_id The transfer identifier
     * @param segment The next segment to send (output)
     * @return SUCCESS if segment available, error otherwise
     */
    TpResult get_next_segment(uint32_t transfer_id, TpSegment& segment);

    /**
     * @brief Handle received TP segment
     *
     * @param segment The received segment
     * @param complete_message Complete reassembled message (output, if available)
     * @return true if segment processed successfully
     */
    bool handle_received_segment(const TpSegment& segment, std::vector<uint8_t>& complete_message);

    /**
     * @brief Acknowledge receipt of segments
     *
     * @param transfer_id The transfer identifier
     * @param segments_acknowledged List of segment offsets that were acknowledged
     * @return SUCCESS if acknowledgment processed
     */
    TpResult acknowledge_segments(uint32_t transfer_id, const std::vector<uint16_t>& segments_acknowledged);

    /**
     * @brief Cancel an ongoing transfer
     *
     * @param transfer_id The transfer to cancel
     * @return SUCCESS if cancelled, error otherwise
     */
    TpResult cancel_transfer(uint32_t transfer_id);

    /**
     * @brief Get transfer status
     *
     * @param transfer_id The transfer identifier
     * @return Current transfer state
     */
    TpTransferState get_transfer_status(uint32_t transfer_id) const;

    /**
     * @brief Set completion callback for transfers
     *
     * @param callback Function called when transfer completes
     */
    void set_completion_callback(TpCompletionCallback callback);

    /**
     * @brief Set progress callback for transfers
     *
     * @param callback Function called to report transfer progress
     */
    void set_progress_callback(TpProgressCallback callback);

    /**
     * @brief Set message callback for completed reassembly
     *
     * @param callback Function called when message is fully reassembled
     */
    void set_message_callback(TpMessageCallback callback);

    /**
     * @brief Process timeouts and cleanup stale transfers
     * Should be called periodically
     */
    void process_timeouts();

    /**
     * @brief Get TP statistics
     *
     * @return Current statistics
     */
    TpStatistics get_statistics() const;

    /**
     * @brief Update TP configuration
     *
     * @param config New configuration
     */
    void update_config(const TpConfig& config);

private:
    TpConfig config_;
    std::unique_ptr<TpSegmenter> segmenter_;
    std::unique_ptr<TpReassembler> reassembler_;

    std::unordered_map<uint32_t, TpTransfer> active_transfers_;
    mutable std::mutex transfers_mutex_;

    TpCompletionCallback completion_callback_;
    TpProgressCallback progress_callback_;
    TpMessageCallback message_callback_;

    uint32_t next_transfer_id_{1};
    TpStatistics statistics_;

    void cleanup_completed_transfers();
    void update_statistics(const TpSegment& segment, bool sent);
};

} // namespace tp
} // namespace someip

#endif // SOMEIP_TP_MANAGER_H
