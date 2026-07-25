/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @implements REQ_PLATFORM_STATIC_003, REQ_PAL_BUFPOOL_ACQUIRE,
 *             REQ_PAL_BUFPOOL_RELEASE, REQ_PAL_BUFPOOL_TIERED,
 *             REQ_PAL_BUFPOOL_EXHAUST_E01
 */

#include "platform/buffer_pool.h"
#include "platform/thread.h"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace someip::platform {

namespace {

constexpr size_t kNumTiers = 3;

constexpr size_t kTierCount[kNumTiers] = {
    SOMEIP_BYTE_POOL_SMALL_COUNT,
    SOMEIP_BYTE_POOL_MEDIUM_COUNT,
    SOMEIP_BYTE_POOL_LARGE_COUNT,
};
constexpr size_t kTierSize[kNumTiers] = {
    SOMEIP_BYTE_POOL_SMALL_SIZE,
    SOMEIP_BYTE_POOL_MEDIUM_SIZE,
    SOMEIP_BYTE_POOL_LARGE_SIZE,
};

// --- Tier 0 (small) ---
alignas(8) static uint8_t slab_0[SOMEIP_BYTE_POOL_SMALL_COUNT * SOMEIP_BYTE_POOL_SMALL_SIZE];
static BufferSlot         slots_0[SOMEIP_BYTE_POOL_SMALL_COUNT];
static uint16_t           free_stack_0[SOMEIP_BYTE_POOL_SMALL_COUNT];

// --- Tier 1 (medium) ---
alignas(8) static uint8_t slab_1[SOMEIP_BYTE_POOL_MEDIUM_COUNT * SOMEIP_BYTE_POOL_MEDIUM_SIZE];
static BufferSlot         slots_1[SOMEIP_BYTE_POOL_MEDIUM_COUNT];
static uint16_t           free_stack_1[SOMEIP_BYTE_POOL_MEDIUM_COUNT];

// --- Tier 2 (large) ---
alignas(8) static uint8_t slab_2[SOMEIP_BYTE_POOL_LARGE_COUNT * SOMEIP_BYTE_POOL_LARGE_SIZE];
static BufferSlot         slots_2[SOMEIP_BYTE_POOL_LARGE_COUNT];
static uint16_t           free_stack_2[SOMEIP_BYTE_POOL_LARGE_COUNT];

static bool in_use_0[SOMEIP_BYTE_POOL_SMALL_COUNT];
static bool in_use_1[SOMEIP_BYTE_POOL_MEDIUM_COUNT];
static bool in_use_2[SOMEIP_BYTE_POOL_LARGE_COUNT];

static uint16_t stack_top[kNumTiers] = {0, 0, 0};

static uint8_t*    slab_ptrs[kNumTiers]       = {slab_0, slab_1, slab_2};
static BufferSlot* slot_arrays[kNumTiers]      = {slots_0, slots_1, slots_2};
static uint16_t*   free_stack_ptrs[kNumTiers]  = {free_stack_0, free_stack_1, free_stack_2};
static bool*       in_use_ptrs[kNumTiers]      = {in_use_0, in_use_1, in_use_2};

static bool pool_initialized{false};

alignas(Mutex) static uint8_t pool_mutex_storage[sizeof(Mutex)];
static Mutex* pool_mutex_ptr{nullptr};

size_t select_tier(size_t requested) {
    for (size_t t = 0; t < kNumTiers; ++t) {
        if (kTierSize[t] >= requested) {
            return t;
        }
    }
    return kNumTiers; // no tier large enough
}

}  // namespace

void init_buffer_pool() {
    if (pool_initialized) { return; }
    pool_mutex_ptr = new (&pool_mutex_storage) Mutex();
    ScopedLock lk(*pool_mutex_ptr);
    for (size_t t = 0; t < kNumTiers; ++t) {
        for (size_t i = 0; i < kTierCount[t]; ++i) {
            slot_arrays[t][i].data     = slab_ptrs[t] + i * kTierSize[t];
            slot_arrays[t][i].capacity = kTierSize[t];
            slot_arrays[t][i].size     = 0;
            slot_arrays[t][i].tier     = static_cast<uint8_t>(t);
            slot_arrays[t][i].index    = static_cast<uint16_t>(i);
            free_stack_ptrs[t][i]      = static_cast<uint16_t>(i);
            in_use_ptrs[t][i]          = false;
        }
        stack_top[t] = static_cast<uint16_t>(kTierCount[t]);
    }
    pool_initialized = true;
}

BufferSlot* acquire_buffer(size_t requested_size) {
    if (requested_size == 0) {
        requested_size = 1;
    }
    ScopedLock lk(*pool_mutex_ptr);
    assert(pool_initialized &&
           "init_static_allocator() must be called before acquire_buffer()");

    size_t best = select_tier(requested_size);
    for (size_t t = best; t < kNumTiers; ++t) {
        if (stack_top[t] > 0) {
            --stack_top[t];
            uint16_t idx = free_stack_ptrs[t][stack_top[t]];
            BufferSlot* s = &slot_arrays[t][idx];
            s->size = 0;
            in_use_ptrs[t][idx] = true;
            return s;
        }
    }
    return nullptr;
}

void release_buffer(BufferSlot* slot) {
    if (!slot) { return; }
    ScopedLock lk(*pool_mutex_ptr);

    uint8_t t = slot->tier;
    if (t >= kNumTiers) { return; }
    if (slot->index >= kTierCount[t]) { return; }
    if (&slot_arrays[t][slot->index] != slot) { return; }
    if (!in_use_ptrs[t][slot->index]) { return; }

    in_use_ptrs[t][slot->index] = false;
    if (stack_top[t] < kTierCount[t]) {
        free_stack_ptrs[t][stack_top[t]] = slot->index;
        ++stack_top[t];
    }
}

}  // namespace someip::platform
