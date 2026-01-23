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

#ifndef SOMEIP_TP_SEGMENTER_H
#define SOMEIP_TP_SEGMENTER_H

#include "tp_types.h"
#include <someip/message.h>
#include <someip/types.h>

namespace someip {
namespace tp {

/**
 * @brief SOME/IP TP Message Segmenter
 *
 * Breaks large messages into smaller segments for transmission over networks
 * with MTU limitations (typically UDP).
 */
class TpSegmenter {
public:
    /**
     * @brief Constructor
     * @param config TP configuration
     */
    explicit TpSegmenter(const TpConfig& config = TpConfig());

    /**
     * @brief Destructor
     */
    ~TpSegmenter();

    // Delete copy and move operations
    TpSegmenter(const TpSegmenter&) = delete;
    TpSegmenter& operator=(const TpSegmenter&) = delete;
    TpSegmenter(TpSegmenter&&) = delete;
    TpSegmenter& operator=(TpSegmenter&&) = delete;

    /**
     * @brief Segment a message into TP segments
     *
     * @param message The message to segment
     * @param segments Output vector for the created segments
     * @return SUCCESS if segmentation successful, error code otherwise
     */
    TpResult segment_message(const Message& message, std::vector<TpSegment>& segments);

    /**
     * @brief Segment raw message data into TP segments
     *
     * @param message_data The raw message data to segment
     * @param segments Output vector for the created segments
     * @return SUCCESS if segmentation successful, error code otherwise
     */
    TpResult segment_data(const std::vector<uint8_t>& message_data, std::vector<TpSegment>& segments);

    /**
     * @brief Update segmentation configuration
     *
     * @param config New configuration
     */
    void update_config(const TpConfig& config);

private:
    TpConfig config_;
    uint8_t next_sequence_number_{0};

    TpResult create_multi_segments(const Message& message,
                                 const std::vector<uint8_t>& payload,
                                 std::vector<TpSegment>& segments);

    void serialize_tp_header(std::vector<uint8_t>& payload, uint16_t offset, bool more_segments);
    MessageType add_tp_flag(MessageType type) const;
};

} // namespace tp
} // namespace someip

#endif // SOMEIP_TP_SEGMENTER_H
