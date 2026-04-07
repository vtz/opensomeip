/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * Shared session manager tests for RTOS and Zephyr test suites.
 *
 * Requires:  CHECK(cond, name)  macro defined before inclusion.
 */

#ifndef SOMEIP_TEST_SESSION_MANAGER_COMMON_INC
#define SOMEIP_TEST_SESSION_MANAGER_COMMON_INC

#include "core/session_manager.h"

static void test_session_manager() {
    using namespace someip;

    printf("\n--- Session Manager tests ---\n");

    SessionManager sm;
    uint16_t s1 = sm.get_next_session_id();
    uint16_t s2 = sm.get_next_session_id();
    CHECK(s2 == s1 + 1, "sequential_ids");

    // Verify IDs are non-zero (session 0 is reserved)
    CHECK(s1 != 0, "first_id_nonzero");
    CHECK(s2 != 0, "second_id_nonzero");
}

#endif // SOMEIP_TEST_SESSION_MANAGER_COMMON_INC
