/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * PAL conformance tests for the static-allocation backend.
 *
 * Threading comes from the POSIX backend (same as host builds).
 * Memory comes from the real static message pool + buffer pool
 * (linked from src/platform/static/{memory,buffer_pool}.cpp).
 *
 * This test verifies that the static-alloc backend satisfies the same PAL
 * contracts as the dynamic backends (POSIX, FreeRTOS, ThreadX, Zephyr).
 *
 * @tests REQ_PAL_MUTEX_LOCK, REQ_PAL_MUTEX_UNLOCK, REQ_PAL_MUTEX_TRYLOCK, REQ_PAL_MUTEX_NONCOPY
 * @tests REQ_PAL_MUTEX_UNLOCK_E01
 * @tests REQ_PAL_CV_WAIT, REQ_PAL_CV_WAIT_PRED, REQ_PAL_CV_NOTIFY_ONE, REQ_PAL_CV_NOTIFY_ALL
 * @tests REQ_PAL_CV_OWNERSHIP
 * @tests REQ_PAL_THREAD_CREATE, REQ_PAL_THREAD_JOINABLE, REQ_PAL_THREAD_JOIN, REQ_PAL_THREAD_NONCOPY
 * @tests REQ_PAL_THREAD_DTOR_E01
 * @tests REQ_PAL_LOCK_ACQUIRE, REQ_PAL_LOCK_RELEASE, REQ_PAL_LOCK_NONCOPY
 * @tests REQ_PAL_SLEEP_DURATION, REQ_PAL_SLEEP_ZERO
 * @tests REQ_PAL_MEM_ALLOC, REQ_PAL_MEM_INDEPENDENT
 * @tests REQ_PLATFORM_STATIC_002, REQ_PLATFORM_STATIC_004
 */

// allocate_message() / release_message() come from the linked
// static/memory.cpp — no stub needed.

#include <gtest/gtest.h>
#include "platform/memory.h"

namespace {
class StaticAllocPalEnv : public ::testing::Environment {
public:
    void SetUp() override {
        someip::platform::init_static_allocator();
    }
};
[[maybe_unused]] auto* const g_env =
    ::testing::AddGlobalTestEnvironment(new StaticAllocPalEnv());
}  // namespace

#include "pal_conformance_tests.inc"
