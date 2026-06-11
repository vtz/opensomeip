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

MessagePtr allocate_message();
void release_message(Message* msg);

}  // namespace platform
}  // namespace someip

#endif // SOMEIP_PLATFORM_STATIC_MEMORY_IMPL_H
