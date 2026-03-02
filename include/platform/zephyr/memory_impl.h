/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_MEMORY_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_MEMORY_IMPL_H

namespace someip {
namespace platform {

MessagePtr allocate_message();
void release_message(Message* msg);

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_ZEPHYR_MEMORY_IMPL_H
