/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_MEMORY_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_MEMORY_IMPL_H

/**
 * @brief Zephyr memory pool backend.
 */

namespace someip {
namespace platform {

/** @implements REQ_PLATFORM_ZEPHYR_002, REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT */
MessagePtr allocate_message();
/** @implements REQ_PLATFORM_ZEPHYR_002, REQ_PAL_MEM_INDEPENDENT */
void release_message(Message* msg);

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_ZEPHYR_MEMORY_IMPL_H
