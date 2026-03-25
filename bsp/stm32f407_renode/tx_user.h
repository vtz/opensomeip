/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ThreadX user configuration for STM32F407 (Cortex-M4) on Renode.
 * This file is included by tx_port.h when TX_INCLUDE_USER_DEFINE_FILE is set.
 ********************************************************************************/

#ifndef TX_USER_H
#define TX_USER_H

/* Use byte pool for dynamic allocation (matches platform::allocate_message) */
#define TX_BYTE_POOL_ENABLE_PERFORMANCE_INFO

/* Timer tick rate: 1ms (same as FreeRTOS config) */
#define TX_TIMER_TICKS_PER_SECOND 1000

/* Disable features not needed for test/demo to reduce code size */
#define TX_DISABLE_NOTIFY_CALLBACKS
#define TX_DISABLE_REDUNDANT_CLEARING
#define TX_DISABLE_ERROR_CHECKING

/* Stack checking in debug builds */
#ifndef NDEBUG
#define TX_ENABLE_STACK_CHECKING
#endif

#endif /* TX_USER_H */
