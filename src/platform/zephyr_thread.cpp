/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/*
 * Zephyr threading wrappers -- compiled only for embedded Zephyr targets
 * (not native_sim). Header-only implementation in platform/thread.h;
 * this file exists for potential future non-inline methods.
 */

#if defined(__ZEPHYR__) && !defined(CONFIG_ARCH_POSIX)

#include "platform/thread.h"

/* Currently all methods are inline in the header. */

#endif
