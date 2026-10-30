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

#ifndef E2E_LIMITS_H
#define E2E_LIMITS_H

/**
 * @brief Compile-time caps for an E2E region stored inside Message.
 *
 * Both values are overridable with -D. Headers or unprotected prefixes
 * above these caps are spec-legal but not representable: protect, validate,
 * and deserialize return Result::NOT_IMPLEMENTED. They are not resized at
 * runtime and are not allocated from the byte pool.
 *
 * SOMEIP_MAX_E2E_HEADER_SIZE must be at least the 12-byte basic profile.
 * 64 bytes leaves room for larger plugins without a per-message heap buffer.
 * SOMEIP_MAX_E2E_PREFIX_SIZE is the unprotected gap between Return Code and
 * the E2E header when Offset is greater than 64 bits (8 bytes of prefix per
 * 64 extra bits).
 */

#ifndef SOMEIP_MAX_E2E_HEADER_SIZE
#define SOMEIP_MAX_E2E_HEADER_SIZE 64  // NOLINT(cppcoreguidelines-macro-usage)
#endif

#ifndef SOMEIP_MAX_E2E_PREFIX_SIZE
#define SOMEIP_MAX_E2E_PREFIX_SIZE 64  // NOLINT(cppcoreguidelines-macro-usage)
#endif

#endif // E2E_LIMITS_H
