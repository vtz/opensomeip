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

#ifndef SOMEIP_TRANSPORT_MESSAGE_REJECTION_H
#define SOMEIP_TRANSPORT_MESSAGE_REJECTION_H

#include "common/result.h"
#include "someip/message.h"
#include "transport/endpoint.h"

#include <cstddef>
#include <cstdint>

namespace someip::transport {

/**
 * @brief Stage at which an incoming PDU was rejected.
 *
 * Local diagnostics only — not an on-wire ReturnCode.
 */
enum class MessageRejectionStage : uint8_t {
    DESERIALIZE = 0,    ///< Message::deserialize rejected the PDU
    TCP_FRAMING = 1,    ///< TCP length/resync rejected a complete frame
    TP_REASSEMBLY = 2,  ///< TP reassembly failed or timed out
    E2E_INTEGRITY = 3   ///< Reserved for receive-path E2E validation (#317)
};

/**
 * @brief Application-visible structural rejection of an incoming message.
 *
 * @implements REQ_TRANSPORT_026
 */
struct MessageRejectionInfo {
    Endpoint sender;
    Result result{Result::MALFORMED_MESSAGE};
    MessageRejectionStage stage{MessageRejectionStage::DESERIALIZE};
    bool has_message_id{false};
    MessageId message_id;
    bool has_request_id{false};
    RequestId request_id;
};

/**
 * @brief Copy Message ID / Request ID from a SOME/IP wire prefix.
 *
 * IDs are reported only when enough header bytes were present to parse them
 * (4 bytes for Message ID, 12 bytes for Request ID). Reads the on-wire
 * big-endian fields so a failed deserialize cannot leave default zeros.
 */
inline void fill_rejection_ids(MessageRejectionInfo& info, const uint8_t* data,
                               size_t wire_size) {
    constexpr size_t message_id_size = 4;
    constexpr size_t request_id_end = 12;
    if (data == nullptr) {
        return;
    }
    if (wire_size >= message_id_size) {
        info.has_message_id = true;
        const uint32_t message_id =
            (static_cast<uint32_t>(data[0]) << 24U) | (static_cast<uint32_t>(data[1]) << 16U) |
            (static_cast<uint32_t>(data[2]) << 8U) | static_cast<uint32_t>(data[3]);
        info.message_id = MessageId::from_uint32(message_id);
    }
    if (wire_size >= request_id_end) {
        info.has_request_id = true;
        const uint32_t request_id =
            (static_cast<uint32_t>(data[8]) << 24U) | (static_cast<uint32_t>(data[9]) << 16U) |
            (static_cast<uint32_t>(data[10]) << 8U) | static_cast<uint32_t>(data[11]);
        info.request_id = RequestId::from_uint32(request_id);
    }
}

/**
 * @brief Copy Message ID / Request ID from a partially parsed Message.
 *
 * IDs are reported only when enough header bytes were present to parse them
 * (4 bytes for Message ID, 12 bytes for Request ID).
 */
inline void fill_rejection_ids(MessageRejectionInfo& info, const Message& msg,
                               size_t wire_size) {
    constexpr size_t message_id_size = 4;
    constexpr size_t request_id_end = 12;
    if (wire_size >= message_id_size) {
        info.has_message_id = true;
        info.message_id = msg.get_message_id();
    }
    if (wire_size >= request_id_end) {
        info.has_request_id = true;
        info.request_id = msg.get_request_id();
    }
}

}  // namespace someip::transport

#endif  // SOMEIP_TRANSPORT_MESSAGE_REJECTION_H
