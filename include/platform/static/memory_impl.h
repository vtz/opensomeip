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

#ifndef SOMEIP_PLATFORM_STATIC_MEMORY_IMPL_H
#define SOMEIP_PLATFORM_STATIC_MEMORY_IMPL_H

/**
 * @brief Static memory pool backend for Message objects.
 *
 * @implements REQ_PLATFORM_STATIC_002, REQ_PAL_MEM_ALLOC,
 *             REQ_PAL_MEM_EXHAUST_E01, REQ_PAL_MEM_THREADSAFE_E01
 */

namespace someip {

class Message;

namespace platform {

/**
 * @brief Initialize all static allocator pools deterministically.
 *
 * @warning MUST be called exactly once at system startup, before any call to
 *          allocate_message() or acquire_buffer(). In debug builds,
 *          calling those functions before initialization triggers an
 *          assertion failure. Typical call sites:
 *          - main() or RTOS entry task for firmware
 *          - GTest ::testing::Environment::SetUp() for host tests
 *
 * Guarantees O(1) WCET on subsequent allocations by removing all
 * lazy-initialization paths.
 *
 * @implements REQ_PLATFORM_STATIC_002, REQ_PLATFORM_STATIC_003
 */
void init_static_allocator();

void init_message_pool();
void init_buffer_pool();

/**
 * @pre init_static_allocator() has been called.
 *      Debug builds assert on this precondition.
 */
MessagePtr allocate_message();
void release_message(Message* msg);

}  // namespace platform
}  // namespace someip

#endif // SOMEIP_PLATFORM_STATIC_MEMORY_IMPL_H
