/**
 * Minimal ThreadX stub for compile-time verification of the
 * ThreadX platform backend.  Not for runtime use.
 */
#ifndef TX_API_H
#define TX_API_H

#include <stdint.h>
#include <stdlib.h>

typedef unsigned int    UINT;
typedef unsigned long   ULONG;
typedef char            CHAR;
typedef unsigned char   UCHAR;

#define TX_SUCCESS          0x00
#define TX_NO_WAIT          0x00
#define TX_WAIT_FOREVER     0xFFFFFFFFUL
#define TX_INHERIT          0x01
#define TX_NO_INHERIT       0x00
#define TX_AUTO_START       0x01
#define TX_NO_TIME_SLICE    0x00
#define TX_OR               0x00
#define TX_OR_CLEAR         0x01
#define TX_TIMER_TICKS_PER_SECOND 100

typedef struct {
    char _pad[64];
} TX_MUTEX;

typedef struct {
    char _pad[64];
} TX_EVENT_FLAGS_GROUP;

typedef struct {
    char _pad[128];
} TX_THREAD;

typedef struct {
    char _pad[64];
} TX_BLOCK_POOL;

static inline UINT tx_mutex_create(TX_MUTEX* m, CHAR* name, UINT inherit) {
    (void)m; (void)name; (void)inherit;
    return TX_SUCCESS;
}

static inline UINT tx_mutex_delete(TX_MUTEX* m) {
    (void)m;
    return TX_SUCCESS;
}

static inline UINT tx_mutex_get(TX_MUTEX* m, ULONG wait) {
    (void)m; (void)wait;
    return TX_SUCCESS;
}

static inline UINT tx_mutex_put(TX_MUTEX* m) {
    (void)m;
    return TX_SUCCESS;
}

static inline UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP* g, CHAR* name) {
    (void)g; (void)name;
    return TX_SUCCESS;
}

static inline UINT tx_event_flags_delete(TX_EVENT_FLAGS_GROUP* g) {
    (void)g;
    return TX_SUCCESS;
}

static inline UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP* g, ULONG flags, UINT option) {
    (void)g; (void)flags; (void)option;
    return TX_SUCCESS;
}

static inline UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP* g, ULONG requested,
                                       UINT get_option, ULONG* actual, ULONG wait) {
    (void)g; (void)requested; (void)get_option; (void)wait;
    if (actual) *actual = requested;
    return TX_SUCCESS;
}

static inline UINT tx_thread_create(TX_THREAD* t, CHAR* name,
                                     void (*entry)(ULONG), ULONG param,
                                     void* stack, ULONG stack_size,
                                     UINT priority, UINT preempt_threshold,
                                     ULONG time_slice, UINT auto_start) {
    (void)t; (void)name; (void)entry; (void)param;
    (void)stack; (void)stack_size; (void)priority;
    (void)preempt_threshold; (void)time_slice; (void)auto_start;
    return TX_SUCCESS;
}

static inline UINT tx_thread_terminate(TX_THREAD* t) {
    (void)t;
    return TX_SUCCESS;
}

static inline UINT tx_thread_delete(TX_THREAD* t) {
    (void)t;
    return TX_SUCCESS;
}

static inline UINT tx_thread_sleep(ULONG ticks) {
    (void)ticks;
    return TX_SUCCESS;
}

static inline UINT tx_block_pool_create(TX_BLOCK_POOL* pool, CHAR* name,
                                         ULONG block_size, void* start,
                                         ULONG size) {
    (void)pool; (void)name; (void)block_size; (void)start; (void)size;
    return TX_SUCCESS;
}

static inline UINT tx_block_allocate(TX_BLOCK_POOL* pool, void** block, ULONG wait) {
    (void)pool; (void)wait;
    static char dummy[256];
    *block = dummy;
    return TX_SUCCESS;
}

static inline UINT tx_block_release(void* block) {
    (void)block;
    return TX_SUCCESS;
}

#endif /* TX_API_H */
