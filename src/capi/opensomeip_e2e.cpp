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
 * @file opensomeip_e2e.cpp
 * @brief C API wrappers for E2E (End-to-End) protection operations.
 * @implements REQ_CAPI_002, REQ_CAPI_003, REQ_CAPI_007, REQ_CAPI_013
 */

#include "capi/opensomeip.h"
#include "capi_internal.h"
#include "e2e/e2e_protection.h"
#include "e2e/e2e_config.h"
#include <new>

struct opensomeip_e2e_s {
    someip::e2e::E2EProtection protection;
};

extern "C" opensomeip_result_t opensomeip_e2e_create(opensomeip_e2e_t** out) {
    if (!out) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        *out = new opensomeip_e2e_s();
        return OPENSOMEIP_RESULT_SUCCESS;
    } catch (...) { *out = nullptr; return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_e2e_destroy(opensomeip_e2e_t* e) {
    if (!e) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try { delete e; return OPENSOMEIP_RESULT_SUCCESS; }
    catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_e2e_protect(opensomeip_e2e_t* e,
                                                       opensomeip_message_t* msg,
                                                       uint16_t data_id,
                                                       uint32_t counter) {
    if (!e || !msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::e2e::E2EConfig config;
        config.data_id = data_id;
        config.max_counter_value = counter;
        auto r = e->protection.protect(msg->msg, config);
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}

extern "C" opensomeip_result_t opensomeip_e2e_check(opensomeip_e2e_t* e,
                                                     const opensomeip_message_t* msg,
                                                     uint16_t data_id) {
    if (!e || !msg) return OPENSOMEIP_RESULT_INVALID_ARGUMENT;
    try {
        someip::e2e::E2EConfig config;
        config.data_id = data_id;
        auto r = e->protection.validate(msg->msg, config);
        return static_cast<opensomeip_result_t>(r);
    } catch (...) { return OPENSOMEIP_RESULT_INTERNAL_ERROR; }
}
