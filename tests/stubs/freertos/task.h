/**
 * Minimal FreeRTOS task stub for compile-time verification.
 */
#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

static inline BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,
    const char* pcName,
    uint32_t usStackDepth,
    void* pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t* pxCreatedTask) {
    (void)pvTaskCode; (void)pcName; (void)usStackDepth;
    (void)pvParameters; (void)uxPriority;
    *pxCreatedTask = (TaskHandle_t)1;
    return pdPASS;
}

static inline void vTaskDelete(TaskHandle_t xTask) { (void)xTask; }
static inline void vTaskDelay(TickType_t xTicksToDelay) { (void)xTicksToDelay; }

#endif // TASK_H
