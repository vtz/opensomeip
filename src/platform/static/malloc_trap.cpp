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
 * @implements REQ_PAL_NOOP_HEAP_VERIFY
 *
 * Link-time heap trap: overrides malloc/free/calloc/realloc so that any
 * heap allocation during protocol operation causes a hard crash.
 *
 * Enabled only when SOMEIP_MALLOC_TRAP is defined (set by CMake when
 * SOMEIP_USE_STATIC_ALLOC=ON and the trap target is built).
 *
 * NOT compiled into the main library; only linked into dedicated
 * trap-verification test binaries.
 */

#ifdef SOMEIP_MALLOC_TRAP

#include <cstddef>
#include <cstdio>
#include <cstdlib>

extern "C" {

// NOLINTBEGIN(readability-identifier-naming,cert-dcl58-cpp)

void* malloc(size_t size) {
    std::fprintf(stderr,
                 "MALLOC TRAP: heap allocation of %zu bytes detected\n", size);
    std::abort();
    return nullptr;  // unreachable
}

void free(void* ptr) {
    if (ptr) {
        std::fprintf(stderr, "MALLOC TRAP: free(%p) detected\n", ptr);
        std::abort();
    }
}

void* calloc(size_t count, size_t size) {
    std::fprintf(stderr,
                 "MALLOC TRAP: calloc(%zu, %zu) detected\n", count, size);
    std::abort();
    return nullptr;
}

void* realloc(void* ptr, size_t size) {
    std::fprintf(stderr,
                 "MALLOC TRAP: realloc(%p, %zu) detected\n", ptr, size);
    std::abort();
    return nullptr;
}

// NOLINTEND(readability-identifier-naming,cert-dcl58-cpp)

}  // extern "C"

#endif  // SOMEIP_MALLOC_TRAP
