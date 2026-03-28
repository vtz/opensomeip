/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_THREADX_MEMORY_INTERNAL_H
#define SOMEIP_PLATFORM_THREADX_MEMORY_INTERNAL_H

/**
 * @brief Internal symbols from the ThreadX memory-pool allocator.
 *
 * Shared between the implementation (memory.cpp) and tests that need
 * direct access to the pool for diagnostics.  Keeping the declarations
 * in one header lets the compiler enforce type consistency.
 */

#include <tx_api.h>

extern TX_BLOCK_POOL message_pool;
extern bool pool_initialized;

#endif // SOMEIP_PLATFORM_THREADX_MEMORY_INTERNAL_H
