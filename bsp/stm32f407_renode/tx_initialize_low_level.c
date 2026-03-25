/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ThreadX low-level initialization for STM32F407 on Renode.
 *
 * Provides _tx_initialize_low_level required by the Cortex-M4 GNU port.
 * Configures SysTick for the ThreadX periodic timer and sets the first
 * unused memory address so tx_application_define receives a valid pointer.
 ********************************************************************************/

#include "tx_api.h"
#include <stdint.h>

extern VOID *_tx_initialize_unused_memory;

#define SYSTICK_CTRL   (*(volatile uint32_t *)0xE000E010)
#define SYSTICK_LOAD   (*(volatile uint32_t *)0xE000E014)
#define SCB_SHPR3      (*(volatile uint32_t *)0xE000ED20)

#define SYSTICK_HZ     168000000U
#define TICK_HZ        100U

extern void *_end;

VOID _tx_initialize_low_level(VOID)
{
    _tx_initialize_unused_memory = (VOID *)&_end;

    SYSTICK_LOAD = (SYSTICK_HZ / TICK_HZ) - 1;
    SYSTICK_CTRL = 0x07;

    /* Set PendSV and SysTick to lowest priority */
    SCB_SHPR3 = 0xFFFF0000U;
}
