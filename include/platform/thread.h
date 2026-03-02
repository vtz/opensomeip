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

#ifndef SOMEIP_PLATFORM_THREAD_H
#define SOMEIP_PLATFORM_THREAD_H

/**
 * @brief Portable threading primitives.
 * @implements REQ_PLATFORM_ARCH_001
 *
 * Platform-specific types (Mutex, ConditionVariable, Thread,
 * this_thread::sleep_for) are provided by the backend's thread_impl.h.
 * The build system sets -I to the correct backend directory so the
 * compiler finds the right implementation.
 */

#include "thread_impl.h"

namespace someip {
namespace platform {

class ScopedLock {
public:
    explicit ScopedLock(Mutex& m) : m_(m) { m_.lock(); }
    ~ScopedLock() { m_.unlock(); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
private:
    Mutex& m_;
};

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_THREAD_H
