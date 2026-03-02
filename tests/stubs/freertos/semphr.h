/**
 * Minimal FreeRTOS semaphore stub for compile-time verification.
 */
#ifndef SEMPHR_H
#define SEMPHR_H

#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (SemaphoreHandle_t)1;
}

static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    return (SemaphoreHandle_t)1;
}

static inline SemaphoreHandle_t xSemaphoreCreateCounting(
    UBaseType_t uxMaxCount, UBaseType_t uxInitialCount) {
    (void)uxMaxCount; (void)uxInitialCount;
    return (SemaphoreHandle_t)1;
}

static inline BaseType_t xSemaphoreTake(
    SemaphoreHandle_t xSemaphore, TickType_t xBlockTime) {
    (void)xSemaphore; (void)xBlockTime;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
    return pdTRUE;
}

static inline void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
}

#endif // SEMPHR_H
