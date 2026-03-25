/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS configuration for STM32F407 (Cortex-M4F) on Renode.
 * Based on the POSIX port config in tests/freertos/FreeRTOSConfig.h,
 * adapted for real ARM Cortex-M4F with hardware FPU.
 ********************************************************************************/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

#define configUSE_PREEMPTION                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMINIMAL_STACK_SIZE                 ((unsigned short)512)
#define configTOTAL_HEAP_SIZE                    ((size_t)(128 * 1024))
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_TRACE_FACILITY                 0
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                10
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIME_SLICING                   1
#define configSTACK_DEPTH_TYPE                   uint32_t

#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1

#define configMAX_PRIORITIES                     56
#define configTIMER_TASK_PRIORITY                (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             (configMINIMAL_STACK_SIZE * 2)

#define configUSE_TIMERS                         1

/* Cortex-M4 specific (no FPU -- Renode's stm32f4 platform uses cortex-m4) */
#define configCPU_CLOCK_HZ                       ((uint32_t)168000000)
#define configPRIO_BITS                          4
#define configKERNEL_INTERRUPT_PRIORITY          (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (5 << (8 - configPRIO_BITS))

#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTimerPendFunctionCall           1
#define INCLUDE_xSemaphoreGetMutexHolder         1

/* Map FreeRTOS port handlers to standard CMSIS names */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#define configASSERT(x)                                                        \
    do {                                                                       \
        if (!(x)) {                                                            \
            printf("ASSERTION FAILED: %s [%s:%d]\n", #x, __FILE__, __LINE__);  \
            fflush(stdout);                                                    \
            while (1) {}                                                       \
        }                                                                      \
    } while (0)

#endif /* FREERTOS_CONFIG_H */
