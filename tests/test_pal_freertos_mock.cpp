/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * PAL conformance tests for the FreeRTOS backend using mock RTOS headers.
 * No real FreeRTOS kernel needed — the mock headers in tests/mocks/freertos/
 * implement the FreeRTOS API using std primitives.
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
 * @tests REQ_PLATFORM_FREERTOS_001, REQ_PLATFORM_FREERTOS_002
 */

#include "someip/message.h"
#include <memory>

namespace someip {
namespace platform {

MessagePtr allocate_message() {
    return std::make_shared<Message>();
}

} // namespace platform
} // namespace someip

#include "pal_conformance_tests.inc"
