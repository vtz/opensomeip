/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SOME/IP event subsystem tests on Zephyr (native_sim).
 *
 * Covers EventPublisher subscription TTL enforcement using the shared
 * platform-independent test suite.  Requires networking (NET_SOCKETS)
 * because EventPublisher and SD depend on the transport layer.
 */

#include <cstdio>
#include <cstring>
#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"

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

#if defined(CONFIG_SOMEIP_EVENTS)
#include "tests/shared/test_sd_subscription_ttl_common.inc"
#endif

int main() {
    printf("=== SOME/IP Event Tests on Zephyr ===\n");

#if defined(CONFIG_SOMEIP_EVENTS)
    test_sd_subscription_ttl();
#else
    printf("  [SKIP] events subsystem not enabled\n");
#endif

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
