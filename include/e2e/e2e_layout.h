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

#ifndef E2E_LAYOUT_H
#define E2E_LAYOUT_H

#include <cstddef>
#include <cstdint>

#include "common/result.h"
#include "e2e/e2e_config.h"
#include "e2e/e2e_header.h"
#include "e2e/e2e_limits.h"

namespace someip::e2e {

inline constexpr size_t kMaxE2EHeaderSize = SOMEIP_MAX_E2E_HEADER_SIZE;
inline constexpr size_t kMaxE2EPrefixSize = SOMEIP_MAX_E2E_PREFIX_SIZE;

static_assert(kMaxE2EHeaderSize >= E2EHeader::get_header_size(),
              "SOMEIP_MAX_E2E_HEADER_SIZE must fit the 12-byte basic profile");
static_assert(kMaxE2EHeaderSize <= 256, "SOMEIP_MAX_E2E_HEADER_SIZE is bounded");
static_assert(kMaxE2EPrefixSize <= 256, "SOMEIP_MAX_E2E_PREFIX_SIZE is bounded");

/**
 * @brief How a receiver locates an E2E header.
 *
 * Offset and header size are configuration, not wire fields. present == false
 * is the historical expect_e2e == false path. present == true with the
 * defaults matches expect_e2e == true (Offset 64 bits, 12-byte header).
 */
struct E2EParseOptions {
    bool present{false};
    uint32_t offset_bits{E2EConfig::DEFAULT_OFFSET_BITS};
    size_t header_size{E2EHeader::get_header_size()};
};

/**
 * @brief Wire index of the first E2E header byte.
 *
 * Request ID is at byte 8. Offset is measured in bits from that byte.
 * Default 64 bits → index 16 (immediately after Return Code).
 */
inline size_t e2e_header_wire_index(uint32_t offset_bits) {
    return static_cast<size_t>(8U + (offset_bits / 8U));
}

/**
 * @brief Accept a byte-aligned layout Message can store.
 *
 * @param prefix_bytes Out: bytes between Return Code and the E2E header.
 *        Zero when offset_bits is 64.
 * @return INVALID_ARGUMENT if Offset overlaps the SOME/IP header
 *         (offset_bits < 64) or is not a multiple of 8, or if header_size
 *         is 0. NOT_IMPLEMENTED if the prefix or header exceeds the
 *         compile-time cap. SUCCESS otherwise.
 *
 * @implements REQ_E2E_PLUGIN_005
 * @satisfies feat_req_someip_102
 * @satisfies feat_req_someip_103
 */
inline Result check_e2e_layout(uint32_t offset_bits, size_t header_size, size_t& prefix_bytes) {
    prefix_bytes = 0;
    if (offset_bits < E2EConfig::DEFAULT_OFFSET_BITS || (offset_bits % 8U) != 0U) {
        return Result::INVALID_ARGUMENT;
    }
    const size_t prefix = static_cast<size_t>((offset_bits - E2EConfig::DEFAULT_OFFSET_BITS) / 8U);
    if (prefix > kMaxE2EPrefixSize) {
        return Result::NOT_IMPLEMENTED;
    }
    if (header_size == 0U) {
        return Result::INVALID_ARGUMENT;
    }
    if (header_size > kMaxE2EHeaderSize) {
        return Result::NOT_IMPLEMENTED;
    }
    prefix_bytes = prefix;
    return Result::SUCCESS;
}

}  // namespace someip::e2e

#endif // E2E_LAYOUT_H
