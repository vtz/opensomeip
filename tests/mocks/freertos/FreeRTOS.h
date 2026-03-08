/*
 * Mock FreeRTOS kernel header for host-based PAL conformance testing.
 * Provides type definitions and macros used by the FreeRTOS PAL backend.
 */

#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <cstdint>
#include <cassert>

typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;

#define pdTRUE   ((BaseType_t)1)
#define pdFALSE  ((BaseType_t)0)
#define pdPASS   pdTRUE
#define pdFAIL   pdFALSE

#define portMAX_DELAY          ((TickType_t)0xFFFFFFFF)
#define configASSERT(x)        do { if (!(x)) assert(false); } while (0)
#define configMINIMAL_STACK_SIZE 128
#define configQUEUE_REGISTRY_SIZE 8
#define tskIDLE_PRIORITY       0

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY ((BaseType_t)-1)

#endif /* MOCK_FREERTOS_H */
