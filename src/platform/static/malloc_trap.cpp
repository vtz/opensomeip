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
 * Armable heap trap: overrides malloc/free/new/delete and aborts when armed.
 * When disarmed (default), calls pass through to the real libc allocator so
 * test-framework code (GTest, std::thread, etc.) can still allocate.
 *
 * Test code arms the trap around protocol operations to verify zero heap
 * allocations, then disarms before test-framework teardown.
 *
 * NOT compiled into the main library; only linked into dedicated
 * trap-verification test binaries.
 */

#ifdef SOMEIP_MALLOC_TRAP

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

extern "C" {

// glibc exposes the real allocator under these symbols
void* __libc_malloc(size_t);
void  __libc_free(void*);
void* __libc_calloc(size_t, size_t);
void* __libc_realloc(void*, size_t);

}  // extern "C"

namespace someip::platform {

static bool g_trap_armed = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void malloc_trap_arm() { g_trap_armed = true; }
void malloc_trap_disarm() { g_trap_armed = false; }
bool malloc_trap_is_armed() { return g_trap_armed; }

}  // namespace someip::platform

extern "C" {

// NOLINTBEGIN(readability-identifier-naming,cert-dcl58-cpp)

void* malloc(size_t size) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: heap allocation of %zu bytes detected\n", size);
        std::abort();
    }
    return __libc_malloc(size);
}

void free(void* ptr) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr, "MALLOC TRAP: free(%p) detected\n", ptr);
        std::abort();
    }
    __libc_free(ptr);
}

void* calloc(size_t count, size_t size) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: calloc(%zu, %zu) detected\n", count, size);
        std::abort();
    }
    return __libc_calloc(count, size);
}

void* realloc(void* ptr, size_t size) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: realloc(%p, %zu) detected\n", ptr, size);
        std::abort();
    }
    return __libc_realloc(ptr, size);
}

// NOLINTEND(readability-identifier-naming,cert-dcl58-cpp)

}  // extern "C"

// NOLINTBEGIN(cert-dcl58-cpp,misc-new-delete-overloads)

void* operator new(std::size_t size) {
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator new(%zu) detected\n", size);
        std::abort();
    }
    void* p = __libc_malloc(size);
    if (!p) { throw std::bad_alloc(); }
    return p;
}

void* operator new[](std::size_t size) {
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator new[](%zu) detected\n", size);
        std::abort();
    }
    void* p = __libc_malloc(size);
    if (!p) { throw std::bad_alloc(); }
    return p;
}

void operator delete(void* ptr) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr, "MALLOC TRAP: operator delete(%p) detected\n", ptr);
        std::abort();
    }
    __libc_free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator delete[](%p) detected\n", ptr);
        std::abort();
    }
    __libc_free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr, "MALLOC TRAP: operator delete(%p, size) detected\n", ptr);
        std::abort();
    }
    __libc_free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator delete[](%p, size) detected\n", ptr);
        std::abort();
    }
    __libc_free(ptr);
}

// NOLINTEND(cert-dcl58-cpp,misc-new-delete-overloads)

#endif  // SOMEIP_MALLOC_TRAP
