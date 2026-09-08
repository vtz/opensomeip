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

#ifndef OPENSOMEIP_CAPI_INTERNAL_H
#define OPENSOMEIP_CAPI_INTERNAL_H

#include "someip/message.h"

#ifdef SOMEIP_STATIC_ALLOC
#include "platform/memory.h"

namespace opensomeip::capi::detail {

inline void ensure_static_pool() {
    static bool done = false;
    if (!done) {
        someip::platform::init_static_allocator();
        done = true;
    }
}

}  // namespace opensomeip::capi::detail

#define CAPI_ENSURE_INIT() opensomeip::capi::detail::ensure_static_pool()
#else
#define CAPI_ENSURE_INIT() ((void)0)
#endif

struct opensomeip_message_s {
    someip::Message msg;
};

#endif /* OPENSOMEIP_CAPI_INTERNAL_H */
