/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_STATIC_MALLOC_TRAP_H
#define SOMEIP_PLATFORM_STATIC_MALLOC_TRAP_H

/**
 * @brief Armable heap trap API for verifying zero-heap operation.
 *
 * @implements REQ_PAL_NOOP_HEAP_VERIFY
 */

namespace someip::platform {

void malloc_trap_arm();
void malloc_trap_disarm();
bool malloc_trap_is_armed();

}  // namespace someip::platform

#endif  // SOMEIP_PLATFORM_STATIC_MALLOC_TRAP_H
