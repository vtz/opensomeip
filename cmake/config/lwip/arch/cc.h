/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal lwIP arch/cc.h for GCC toolchains (host and cross-compile).
 * A real product should provide its own arch/cc.h in its BSP.
 ********************************************************************************/

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>

#define LWIP_PLATFORM_DIAG(x) do { printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) \
    do { printf("lwIP assert: %s\n", x); abort(); } while(0)

#endif /* LWIP_ARCH_CC_H */
