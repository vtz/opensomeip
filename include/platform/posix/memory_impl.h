/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_MEMORY_IMPL_H
#define SOMEIP_PLATFORM_POSIX_MEMORY_IMPL_H

namespace someip {
namespace platform {

inline MessagePtr allocate_message() {
    return std::make_shared<Message>();
}

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_POSIX_MEMORY_IMPL_H
