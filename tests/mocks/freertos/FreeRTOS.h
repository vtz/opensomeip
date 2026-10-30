/*
 * Mock FreeRTOS kernel header for host-based PAL conformance testing.
 * Provides type definitions and macros used by the FreeRTOS PAL backend.
 */

#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <cstdint>
#include <cassert>
#include <limits>

typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;
#if defined(configUSE_16_BIT_TICKS) && configUSE_16_BIT_TICKS == 1
typedef uint16_t TickType_t;
#else
typedef uint32_t TickType_t;
#endif
typedef uint32_t StackType_t;

#define pdTRUE   ((BaseType_t)1)
#define pdFALSE  ((BaseType_t)0)
#define pdPASS   pdTRUE
#define pdFAIL   pdFALSE

#define portMAX_DELAY (std::numeric_limits<TickType_t>::max())
#define configASSERT(x)        do { if (!(x)) assert(false); } while (0)
#define configMINIMAL_STACK_SIZE 128
#define configQUEUE_REGISTRY_SIZE 8
#define tskIDLE_PRIORITY       0

#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000
#endif
#define pdMS_TO_TICKS(ms) ((TickType_t)(((ms) * configTICK_RATE_HZ) / 1000))
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY ((BaseType_t)-1)

#endif /* MOCK_FREERTOS_H */
