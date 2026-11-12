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
 * @file test_capi_pure_c.c
 * @brief Pure-C consumer of the public C API header.
 *
 * @test_case TC_CAPI_PURE_C_CONSUMER
 * @tests REQ_CAPI_001
 *
 * This translation unit is compiled as C. It includes opensomeip.h and calls
 * one function so the link succeeds. REQ_CAPI_001 is not met by the C++ tests.
 */

#include "capi/opensomeip.h"

int main(void) {
    const uint32_t expected =
        ((uint32_t)OPENSOMEIP_CAPI_VERSION_MAJOR << 16) |
        ((uint32_t)OPENSOMEIP_CAPI_VERSION_MINOR << 8) |
        (uint32_t)OPENSOMEIP_CAPI_VERSION_PATCH;

    if (opensomeip_capi_version() != expected) {
        return 1;
    }
    return 0;
}
