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

#ifndef SOMEIP_PLATFORM_MEMORY_H
#define SOMEIP_PLATFORM_MEMORY_H

/**
 * @brief Portable memory management for Message objects.
 *
 * On host / native_sim, Message objects are allocated via std::make_shared.
 * On embedded Zephyr targets, a static pool (k_mem_slab) can be used to
 * avoid heap fragmentation for the most frequently allocated type.
 */

#include "someip/message.h"
#include <memory>

namespace someip {
namespace platform {

#if defined(__ZEPHYR__) && !defined(CONFIG_ARCH_POSIX)

MessagePtr allocate_message();
void release_message(Message* msg);

#else

inline MessagePtr allocate_message() {
    return std::make_shared<Message>();
}

#endif

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_MEMORY_H
