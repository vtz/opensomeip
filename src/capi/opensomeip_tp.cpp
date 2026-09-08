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

/**
 * @file opensomeip_tp.cpp
 * @brief C API wrappers for Transport Protocol (TP) operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_007, REQ_CAPI_013
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "tp/tp_manager.h"
#include <cstring>

struct opensomeip_tp_manager_s {
    someip::tp::TpManager manager;
};

extern "C" opensomeip_result_t opensomeip_tp_manager_create(opensomeip_tp_manager_t** out) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    CAPI_ENSURE_INIT();
    try {
        *out = new opensomeip_tp_manager_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tp_manager_destroy(opensomeip_tp_manager_t* tp) {
    if (!tp) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete tp; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tp_manager_initialize(opensomeip_tp_manager_t* tp) {
    if (!tp) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        return tp->manager.initialize() ? OPENSOMEIP_RESULT_SUCCESS : OPENSOMEIP_RESULT_INTERNAL_ERROR;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tp_manager_shutdown(opensomeip_tp_manager_t* tp) {
    if (!tp) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { tp->manager.shutdown(); return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tp_needs_segmentation(const opensomeip_tp_manager_t* tp,
                                                                 const uint8_t* payload, size_t len,
                                                                 int* out_needs) {
    if (!tp || !out_needs) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::Message msg;
        if (payload && len > 0) {
            msg.set_payload(payload, len);
        }
        *out_needs = tp->manager.needs_segmentation(msg) ? 1 : 0;
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tp_segment(opensomeip_tp_manager_t* tp,
                                                      const opensomeip_message_t* msg,
                                                      uint8_t* out_buf, size_t* out_len) {
    if (!tp || !msg || !out_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        uint32_t transfer_id = 0;
        auto result = tp->manager.segment_message(msg->msg, transfer_id);
        if (result != someip::tp::TpResult::SUCCESS) {
            return OPENSOMEIP_RESULT_INTERNAL_ERROR;
        }

        size_t written = 0;
        someip::tp::TpSegment segment;
        while (tp->manager.get_next_segment(transfer_id, segment) == someip::tp::TpResult::SUCCESS) {
            if (segment.payload.empty()) {
                break;
            }
            size_t seg_size = segment.payload.size();
            if (out_buf && written + sizeof(uint32_t) + seg_size <= *out_len) {
                uint32_t seg_len = static_cast<uint32_t>(seg_size);
                std::memcpy(out_buf + written, &seg_len, sizeof(uint32_t));
                written += sizeof(uint32_t);
                std::memcpy(out_buf + written, segment.payload.data(), seg_size);
                written += seg_size;
            } else {
                *out_len = written;
                return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
            }
        }
        *out_len = written;
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_tp_reassemble(opensomeip_tp_manager_t* tp,
                                                         const uint8_t* segment_data, size_t segment_len,
                                                         uint8_t* out_buf, size_t* out_len,
                                                         int* complete) {
    if (!tp || !complete || !out_len) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    if (segment_len > 0 && !segment_data) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::tp::TpSegment seg;
        seg.payload.resize(segment_len);
        if (segment_len > 0) {
            std::memcpy(seg.payload.data(), segment_data, segment_len);
        }
        seg.header.segment_length = static_cast<uint16_t>(segment_len);
        seg.header.message_length = static_cast<uint32_t>(segment_len);
        seg.header.message_type = someip::tp::TpMessageType::SINGLE_MESSAGE;

        someip::platform::ByteBuffer complete_msg;
        bool done = tp->manager.handle_received_segment(seg, complete_msg);

        if (done && !complete_msg.empty()) {
            if (*out_len < complete_msg.size()) {
                *complete = 0;
                *out_len = complete_msg.size();
                return OPENSOMEIP_RESULT_BUFFER_OVERFLOW;
            }
            *complete = 1;
            if (out_buf) {
                std::memcpy(out_buf, complete_msg.data(), complete_msg.size());
            }
            *out_len = complete_msg.size();
        } else {
            *complete = 0;
            *out_len = 0;
        }
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
