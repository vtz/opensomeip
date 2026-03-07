/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_MEMORY_IMPL_H
#define SOMEIP_PLATFORM_POSIX_MEMORY_IMPL_H

/**
 * @brief POSIX/Host memory backend.
 */

namespace someip {
namespace platform {

/** @implements REQ_PLATFORM_POSIX_002, REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT */
inline MessagePtr allocate_message() {
    return std::make_shared<Message>();
}

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_POSIX_MEMORY_IMPL_H
