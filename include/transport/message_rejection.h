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
    MessageId message_id{};
    bool has_request_id{false};
    RequestId request_id{};
};

/**
 * @brief Copy Message ID / Request ID from a partially parsed Message.
 *
 * IDs are reported only when enough header bytes were present to parse them
 * (4 bytes for Message ID, 12 bytes for Request ID).
 */
inline void fill_rejection_ids(MessageRejectionInfo& info, const Message& msg,
                               size_t wire_size) {
    constexpr size_t kMessageIdSize = 4;
    constexpr size_t kRequestIdEnd = 12;
    if (wire_size >= kMessageIdSize) {
        info.has_message_id = true;
        info.message_id = msg.get_message_id();
    }
    if (wire_size >= kRequestIdEnd) {
        info.has_request_id = true;
        info.request_id = msg.get_request_id();
    }
}

}  // namespace someip::transport

#endif  // SOMEIP_TRANSPORT_MESSAGE_REJECTION_H
