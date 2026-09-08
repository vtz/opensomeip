/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_FAKE_BUFFER_POOL_IMPL_H
#define SOMEIP_FAKE_BUFFER_POOL_IMPL_H

/**
 * @brief Byte-buffer type for protocol-layer unit tests.
 *
 * Compile protocol code with -I tests/fakes/ to shadow buffer_pool_impl.h.
 * Uses the dynamic (heap-backed) backend so existing tests keep working.
 */

#include "../../include/platform/dynamic/buffer_pool_impl.h"

#endif // SOMEIP_FAKE_BUFFER_POOL_IMPL_H
