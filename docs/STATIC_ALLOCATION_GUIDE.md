<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Static Allocation Integrator Guide

This document describes the contract integrators must follow when building
OpenSOME/IP with the static allocation backend (`SOMEIP_USE_STATIC_ALLOC=ON`).

## 1. Initialization — `init_static_allocator()`

**You must call `someip::platform::init_static_allocator()` exactly once
at system startup, before any call to `allocate_message()`,
`acquire_buffer()`, or any API that internally allocates messages or
buffers.**

```cpp
#include "platform/memory.h"

int main() {
    someip::platform::init_static_allocator();
    // ... application code ...
}
```

On FreeRTOS / ThreadX, call it at the beginning of your application task
(after the scheduler is running), not from a C++ static constructor —
the mutex backing the pools requires a live RTOS heap.

`init_static_allocator()` is **idempotent**: calling it more than once is
safe (subsequent calls are no-ops).  This is useful in test harnesses where
multiple test environments may each call init.

In debug builds, calling `allocate_message()` or `acquire_buffer()` before
initialization triggers an assertion failure.

## 2. Pool Sizing — Defaults vs. Constrained Targets

All pool sizes are defined in `include/platform/static/static_config.h`
and can be overridden via CMake `-D` flags or CMakePresets cache variables.

### Host Defaults (suitable for Linux / macOS / Windows)

| Parameter | Default | Description |
|---|---|---|
| `SOMEIP_MESSAGE_POOL_SIZE` | 16 | Max concurrent `Message` objects |
| `SOMEIP_BYTE_POOL_SMALL_COUNT` | 32 | Small buffer slots (256 B each) |
| `SOMEIP_BYTE_POOL_MEDIUM_COUNT` | 16 | Medium buffer slots (1500 B each) |
| `SOMEIP_BYTE_POOL_LARGE_COUNT` | 4 | Large buffer slots (64 KB each) |
| `SOMEIP_BYTE_POOL_LARGE_SIZE` | 65536 | Large buffer tier size |
| `SOMEIP_MAX_TP_SEGMENTS` | 64 | Max TP segments per transfer |
| `SOMEIP_MAX_TP_REASSEMBLY_SIZE` | 2048 | Max TP reassembly byte tracking |
| `SOMEIP_DEFAULT_MAP_CAPACITY` | 32 | Default `UnorderedMap` capacity |
| `SOMEIP_DEFAULT_VECTOR_CAPACITY` | 32 | Default `Vector` capacity |
| `SOMEIP_DEFAULT_STRING_CAPACITY` | 64 | Default `String` capacity |

### Renode / STM32F407 (256 KB SRAM) Tunings

The CMakePresets `freertos-cortexm4-renode-static` and
`threadx-cortexm4-renode-static` use reduced values to fit within 256 KB
SRAM:

| Parameter | Renode Value | Rationale |
|---|---|---|
| `SOMEIP_MESSAGE_POOL_SIZE` | 8 | Halved to save BSS space |
| `SOMEIP_BYTE_POOL_SMALL_COUNT` | 16 | Halved |
| `SOMEIP_BYTE_POOL_LARGE_COUNT` | 8 | Increased count at smaller tier size |
| `SOMEIP_BYTE_POOL_LARGE_SIZE` | 4096 | Reduced from 64 KB — avoids SRAM overflow |
| `SOMEIP_MAX_TP_SEGMENTS` | 16 | Reduces per-transfer memory |
| `SOMEIP_MAX_TP_REASSEMBLY_SIZE` | 1500 | Single-MTU reassembly tracking |
| `SOMEIP_DEFAULT_MAP_CAPACITY` | 4 | Minimal map buckets |
| `SOMEIP_FREERTOS_HEAP_SIZE` | 40960 | FreeRTOS heap for RTOS primitives |

**Key rule:** the defaults are designed for host builds with generous RAM.
When targeting MCUs, you **must** tune these values for your SRAM budget.
Use `scripts/static_memory_budget.py` to estimate total BSS footprint
before building.

## 3. Build Configuration

```bash
# Host static-alloc build
cmake -B build -DSOMEIP_USE_STATIC_ALLOC=ON
cmake --build build
ctest --test-dir build

# Cross-compile for Renode (using presets)
cmake --preset freertos-cortexm4-renode-static
cmake --build --preset freertos-cortexm4-renode-static
```

When `SOMEIP_USE_STATIC_ALLOC=ON`:
- The ETL (Embedded Template Library) submodule is required.
- `BUILD_EXAMPLES` is set to `OFF` in all static presets — examples use
  STL containers and are not compatible with the static backend.
- The `someip_malloc_trap` object library is only built on non-cross-
  compiling hosts (it requires `dlfcn.h`).

## 4. Known Heap Allocations Under Static-Alloc

Even with `SOMEIP_USE_STATIC_ALLOC=ON`, these locations still use the
heap intentionally:

| Location | Allocation | Justification |
|---|---|---|
| `e2e_profiles/standard_profile.cpp` | `std::make_unique<BasicE2EProfile>()` | One-time startup registration; occurs before malloc trap is armed |
| `Message::to_string()`, `Endpoint::to_string()` | `std::string` / `std::stringstream` | Debug / diagnostic only; not on data path |
| `to_string(Result)`, `to_string(MessageType)` | `std::string` | Debug / diagnostic only |

These are not called on the hot path and do not violate the zero-heap
guarantee for message allocation, serialization, and transport operations.

## 5. Thread Safety

- All pool operations (`allocate_message`, `acquire_buffer`,
  `release_message`, `release_buffer`) are mutex-protected and safe to
  call from multiple threads / tasks.
- The pool mutex is created via placement-new inside
  `init_static_allocator()`, not as a file-scope C++ static, to avoid
  static initialization order issues on bare-metal RTOS targets.

## 6. Capacity Exhaustion Behavior

When a pool or bounded container is full:

- `allocate_message()` returns a null `MessagePtr`.
- `acquire_buffer()` returns `nullptr`.
- `ByteBuffer::push_back()` / `resize()` / `insert()` leave the buffer
  unchanged when the pool cannot provide a larger slot.
- `SD add_entry()` / `add_option()` return `false`.
- Event publisher `handle_subscription_locked()` returns `false` if
  filter count exceeds capacity.

Callers must check return values.  In debug builds, ETL containers
trigger the registered ETL error handler on overflow; in production
builds with `ETL_THROW_EXCEPTIONS=0`, overflow is a silent no-op by
default (the custom ETL error handler logs and increments a counter).
