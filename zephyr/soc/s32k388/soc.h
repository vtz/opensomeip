/*
 * Copyright (c) 2025 Vinicius Tadeu Zein
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal CMSIS device header for NXP S32K388 Renode simulation.
 * Provides the macros and IRQ enum that CMSIS core_cm7.h requires,
 * without depending on the NXP HAL.
 */

#ifndef SOC_S32K388_SOC_H_
#define SOC_S32K388_SOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#define __CM7_REV                 0x0000U
#define __NVIC_PRIO_BITS          4U
#define __Vendor_SysTickConfig    0U
#define __MPU_PRESENT             1U
#define __FPU_PRESENT             1U
#define __FPU_DP                  0U
#define __ICACHE_PRESENT          0U
#define __DCACHE_PRESENT          0U
#define __DTCM_PRESENT            0U
#define __ITCM_PRESENT            0U
#define __DSP_PRESENT             0U
#define __VTOR_PRESENT            1U

typedef enum {
	NonMaskableInt_IRQn   = -14,
	HardFault_IRQn        = -13,
	MemoryManagement_IRQn = -12,
	BusFault_IRQn         = -11,
	UsageFault_IRQn       = -10,
	SVCall_IRQn           = -5,
	DebugMonitor_IRQn     = -4,
	PendSV_IRQn           = -2,
	SysTick_IRQn          = -1,
} IRQn_Type;

#include <core_cm7.h>

#ifdef __cplusplus
}
#endif

#endif /* SOC_S32K388_SOC_H_ */
