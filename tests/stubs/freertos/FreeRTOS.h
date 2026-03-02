/**
 * Minimal FreeRTOS stub for compile-time verification of the
 * FreeRTOS platform backend.  Not for runtime use.
 */
#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>
#include <stdlib.h>

#define configASSERT(x) ((void)(x))
#define configQUEUE_REGISTRY_SIZE 8
#define portMAX_DELAY 0xFFFFFFFFUL
#define tskIDLE_PRIORITY 0
#define pdPASS 1
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(xTimeInMs) ((uint32_t)(xTimeInMs))

typedef uint32_t TickType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t StackType_t;

#endif // FREERTOS_H
