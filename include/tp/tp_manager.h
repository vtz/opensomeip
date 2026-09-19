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

#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/thread.h"

#include "../someip/message.h"
#include "common/result.h"
#include <atomic>
#include <cstddef>
#include <optional>

#include "tp_segmenter.h"
#include "tp_reassembler.h"

namespace someip::tp {

/**
 * @brief Atomic statistics counters for lock-free diagnostic reads.
 *
 * All fields use @c std::atomic<uint32_t> with @c memory_order_relaxed so
 * that a diagnostic thread can call @c get_sender_statistics() or
 * @c get_receiver_statistics() without locking, while the owning path
 * (send or receive) is actively incrementing.
 *
 * Reads (snapshots) use relaxed loads, which on ARM and x86 compile to
 * plain load instructions.  Writes use @c fetch_add with relaxed
 * ordering, which is an atomic read-modify-write; the generated
 * instructions and cost depend on the compiler and target (e.g.
 * @c lock @c xadd on x86, @c ldxr/@c stxr loop on ARM).
 */
struct AtomicTpStatistics {
    std::atomic<uint32_t> messages_segmented{0};
    std::atomic<uint32_t> messages_reassembled{0};
    std::atomic<uint32_t> segments_sent{0};
    std::atomic<uint32_t> segments_received{0};
    std::atomic<uint32_t> retransmissions{0};
    std::atomic<uint32_t> timeouts{0};
    std::atomic<uint32_t> errors{0};

    /**
     * @brief Return a point-in-time snapshot as a plain copyable POD struct.
     *
     * Each field is read with @c memory_order_relaxed; the snapshot is not
     * guaranteed to be a consistent cut across all fields, but each
     * individual counter is race-free.
     */
    TpStatistics snapshot() const {
        return {
            messages_segmented.load(std::memory_order_relaxed),
            messages_reassembled.load(std::memory_order_relaxed),
            segments_sent.load(std::memory_order_relaxed),
            segments_received.load(std::memory_order_relaxed),
            retransmissions.load(std::memory_order_relaxed),
            timeouts.load(std::memory_order_relaxed),
            errors.load(std::memory_order_relaxed)
        };
    }
};

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
    bool handle_received_segment(const TpSegment& segment, platform::ByteBuffer& complete_message);

    /**
     * @brief Ingest a raw UDP datagram that has the TP flag set
     *
     * Parses the wire TP header, feeds the reassembler, and if a complete
     * message is ready fills @p out_complete with TP flag cleared and the
     * reassembled payload. @p sender_ipv4 / @p sender_port are part of the
     * reassembly key so two peers cannot collide.
     *
     * @return true if a complete valid message is ready
     *
     * @param ingest_error When non-null, set to SUCCESS on incomplete reassembly
     *        (process_segment accepted the segment but the message is not
     *        complete) and to a failure code when the datagram is structurally
     *        rejected, including process_segment failures. Incomplete must not
     *        be reported as a transport rejection.
     * @return true if a complete message is ready
     * @implements REQ_TP_055, REQ_TP_078, REQ_TP_091
     * @satisfies feat_req_someiptp_785
     */
    bool ingest_datagram(const uint8_t* data, size_t size, Message& out_complete,
                         uint32_t sender_ipv4 = 0, uint16_t sender_port = 0,
                         Result* ingest_error = nullptr);

    /**
     * @brief Segment a message and return wire datagrams (segment payloads)
     * @implements REQ_TP_050, REQ_TP_090
     */
    TpResult segment_and_serialize(const Message& message, TpSegmentVector& segments);

    /**
     * @brief Acknowledge receipt of segments
     *
     * @param transfer_id The transfer identifier
     * @param segments_acknowledged List of segment offsets that were acknowledged
     * @return SUCCESS if acknowledgment processed
     */
    TpResult acknowledge_segments(uint32_t transfer_id, const platform::Vector<uint16_t>& segments_acknowledged);

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
     * @brief Get sender-path statistics (segmentation) — point-in-time snapshot
     *
     * Returns a plain @c TpStatistics with the current values of counters
     * written by the sender path: @c messages_segmented, @c segments_sent,
     * @c errors.  Safe to call concurrently from any thread.
     *
     * @return Snapshot of sender statistics
     */
    TpStatistics get_sender_statistics() const;

    /**
     * @brief Get receiver-path statistics (reassembly) — point-in-time snapshot
     *
     * Returns a plain @c TpStatistics with the current values of counters
     * written by the receiver path: @c messages_reassembled,
     * @c segments_received, @c timeouts, @c retransmissions, @c errors.
     * Safe to call concurrently from any thread.
     *
     * @return Snapshot of receiver statistics
     */
    TpStatistics get_receiver_statistics() const;

    /**
     * @brief Update TP configuration
     *
     * @param config New configuration
     */
    void update_config(const TpConfig& config);

private:
    TpConfig config_;
    std::optional<TpSegmenter> segmenter_;
    std::optional<TpReassembler> reassembler_;

    platform::UnorderedMap<uint32_t, TpTransfer> active_transfers_;
    mutable platform::Mutex transfers_mutex_;

    TpCompletionCallback completion_callback_;
    TpProgressCallback progress_callback_;
    TpMessageCallback message_callback_;

    uint32_t next_transfer_id_{1};
    AtomicTpStatistics sender_statistics_;
    AtomicTpStatistics receiver_statistics_;

    void cleanup_completed_transfers();
};

}  // namespace someip::tp

#endif // SOMEIP_TP_MANAGER_H
