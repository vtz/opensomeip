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

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/thread.h"

#include <cstddef>

namespace someip::tp {

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
    bool process_segment(const TpSegment& segment, platform::ByteBuffer& complete_message);

    /**
     * @brief Check if any buffer with this message_id is being reassembled
     *
     * Scans all active buffers and returns true if at least one has a
     * matching message_id.  When multiple transfers share a message_id
     * (different client/session/message-type), this cannot distinguish
     * them — use the TpReassemblyKey overload for per-transfer queries.
     *
     * @param message_id The message identifier
     * @return true if at least one matching reassembly is in progress
     */
    bool is_reassembling(uint32_t message_id) const;

    /**
     * @brief Check if a specific transfer (exact composite key) is active
     *
     * O(1) lookup; unambiguous when multiple transfers share a message_id.
     *
     * @param key The full reassembly key
     * @return true if the exact transfer is in progress
     */
    bool is_reassembling(const TpReassemblyKey& key) const;

    /**
     * @brief Get reassembly progress for the first buffer matching message_id
     *
     * When multiple buffers share message_id, returns the first match found
     * (iteration order is unspecified).  Use the TpReassemblyKey overload
     * for deterministic per-transfer progress.
     *
     * @param message_id The message identifier
     * @param received_bytes Number of bytes received so far (output)
     * @param total_bytes Total expected bytes (output)
     * @return true if a matching buffer was found
     */
    bool get_reassembly_progress(uint32_t message_id, uint32_t& received_bytes, uint32_t& total_bytes) const;

    /**
     * @brief Get reassembly progress for an exact transfer
     *
     * @param key The full reassembly key
     * @param received_bytes Number of bytes received so far (output)
     * @param total_bytes Total expected bytes (output)
     * @return true if the transfer exists
     */
    bool get_reassembly_progress(const TpReassemblyKey& key, uint32_t& received_bytes, uint32_t& total_bytes) const;

    /**
     * @brief Cancel all reassembly buffers matching message_id
     *
     * Erases every buffer whose key.message_id equals the argument.
     * Use the TpReassemblyKey overload to cancel a single transfer.
     *
     * @param message_id The message identifier
     */
    void cancel_reassembly(uint32_t message_id);

    /**
     * @brief Cancel reassembly for one exact transfer
     *
     * @param key The full reassembly key
     */
    void cancel_reassembly(const TpReassemblyKey& key);

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
    platform::UnorderedMap<TpReassemblyKey, TpReassemblyBuffer, 16, TpReassemblyKeyHash> reassembly_buffers_;
    mutable platform::Mutex config_mutex_;
    mutable platform::Mutex buffers_mutex_;

    TpConfig get_config_copy() const;
    bool validate_segment(const TpSegment& segment) const;
    TpReassemblyBuffer* find_or_create_buffer(const TpSegment& segment);
    bool add_segment_to_buffer(TpReassemblyBuffer& buffer, const TpSegment& segment);
    void cleanup_completed_buffers();
    void cleanup_timed_out_buffers(const TpConfig& config);
    bool parse_tp_header(const platform::ByteBuffer& payload, uint32_t& offset, bool& more_segments) const;
};

}  // namespace someip::tp

#endif // SOMEIP_TP_REASSEMBLER_H
