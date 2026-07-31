/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_DYNAMIC_BUFFER_POOL_IMPL_H
#define SOMEIP_PLATFORM_DYNAMIC_BUFFER_POOL_IMPL_H

/**
 * @brief Dynamic (heap-backed) byte buffer type.
 */

#include <cstdint>
#include <vector>

namespace someip::platform {

using ByteBuffer = std::vector<uint8_t>;

}  // namespace someip::platform

#endif // SOMEIP_PLATFORM_DYNAMIC_BUFFER_POOL_IMPL_H
