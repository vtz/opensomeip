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

/* Pre-define lwip_htons/lwip_htonl as inline intrinsics so that lwip/def.h
   skips its function declarations (guarded by #ifndef).  This avoids a
   link-time dependency on lwIP's def.c when using headers-only mode. */
#define lwip_htons(x) ((u16_t)__builtin_bswap16((u16_t)(x)))
#define lwip_htonl(x) ((u32_t)__builtin_bswap32((u32_t)(x)))

#endif /* LWIP_ARCH_CC_H */
