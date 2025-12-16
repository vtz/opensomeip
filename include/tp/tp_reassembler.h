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

#ifndef SOMEIP_TP_REASSEMBLER_H
#define SOMEIP_TP_REASSEMBLER_H

#include "tp_types.h"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace someip {
namespace tp {

/**
 * @brief SOME/IP TP Message Reassembler
 *
 * Reassembles TP segments back into complete messages on the receiving side.
 * Handles out-of-order delivery and duplicate segments.
 */
class TpReassembler {
public:
    /**
     * @brief Constructor
     * @param config TP configuration
     */
    explicit TpReassembler(const TpConfig& config = TpConfig());

    /**
     * @brief Destructor
     */
    ~TpReassembler();

    // Delete copy and move operations
    TpReassembler(const TpReassembler&) = delete;
    TpReassembler& operator=(const TpReassembler&) = delete;
    TpReassembler(TpReassembler&&) = delete;
    TpReassembler& operator=(TpReassembler&&) = delete;

    /**
     * @brief Process a received TP segment
     *
     * @param segment The received segment
     * @param complete_message Complete reassembled message (output, if available)
     * @return true if segment processed successfully, false on error
     */
    bool process_segment(const TpSegment& segment, std::vector<uint8_t>& complete_message);

    /**
     * @brief Check if a message is currently being reassembled
     *
     * @param message_id The message identifier
     * @return true if reassembly is in progress
     */
    bool is_reassembling(uint32_t message_id) const;

    /**
     * @brief Get reassembly progress for a message
     *
     * @param message_id The message identifier
     * @param received_bytes Number of bytes received (output)
     * @param total_bytes Total expected bytes (output)
     * @return true if message found, false otherwise
     */
    bool get_reassembly_progress(uint32_t message_id, uint32_t& received_bytes, uint32_t& total_bytes) const;

    /**
     * @brief Cancel reassembly for a message
     *
     * @param message_id The message identifier
     */
    void cancel_reassembly(uint32_t message_id);

    /**
     * @brief Process timeouts and cleanup stale reassembly buffers
     * Should be called periodically
     */
    void process_timeouts();

    /**
     * @brief Get number of active reassembly operations
     *
     * @return Number of messages currently being reassembled
     */
    size_t get_active_reassemblies() const;

    /**
     * @brief Update reassembly configuration
     *
     * @param config New configuration
     */
    void update_config(const TpConfig& config);

private:
    TpConfig config_;
    std::unordered_map<uint32_t, std::unique_ptr<TpReassemblyBuffer>> reassembly_buffers_;
    mutable std::mutex config_mutex_;
    mutable std::mutex buffers_mutex_;

    TpConfig get_config_copy() const;
    bool validate_segment(const TpSegment& segment) const;
    TpReassemblyBuffer* find_or_create_buffer(const TpSegment& segment);
    bool add_segment_to_buffer(TpReassemblyBuffer& buffer, const TpSegment& segment);
    void cleanup_completed_buffers();
    void cleanup_timed_out_buffers(const TpConfig& config);
};

} // namespace tp
} // namespace someip

#endif // SOMEIP_TP_REASSEMBLER_H

