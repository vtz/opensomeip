/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SOME/IP core tests on Zephyr (native_sim and hardware targets).
 *
 * Covers Message, Endpoint, SessionManager, Serializer, E2E, and TP
 * using the shared platform-independent test suites.
 */

#include <cstdio>
#include <cstring>
#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"
#include "core/session_manager.h"
#include "serialization/serializer.h"

using namespace someip;
using namespace someip::transport;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, name)                                   \
    do {                                                    \
        if (cond) {                                         \
            printf("  [PASS] %s\n", name);                  \
            tests_passed++;                                 \
        } else {                                            \
            printf("  [FAIL] %s\n", name);                  \
            tests_failed++;                                 \
        }                                                   \
    } while (0)

// Shared platform-independent test suites
#include "tests/shared/test_message_common.inc"
#include "tests/shared/test_endpoint_common.inc"
#include "tests/shared/test_session_manager_common.inc"
#include "tests/shared/test_serializer_common.inc"
#include "tests/shared/test_e2e_common.inc"
#include "tests/shared/test_tp_common.inc"
#include "tests/shared/test_sd_subscription_ttl_common.inc"

int main() {
    printf("=== SOME/IP Core Tests on Zephyr ===\n");

    test_message();
    test_endpoint();
    test_session_manager();
    test_serializer();
    test_e2e();
    test_tp();
    test_sd_subscription_ttl();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
