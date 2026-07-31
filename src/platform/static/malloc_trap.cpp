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
 * Armable heap trap: overrides malloc/free/new/delete.
 *
 * When armed, any heap allocation triggers __builtin_trap() (portable
 * across GCC, Clang, MSVC — no glibc dependency).
 *
 * When disarmed (default), calls pass through to the real allocator via
 * dlsym(RTLD_NEXT, "malloc") so test-framework code (GTest, std::thread,
 * etc.) can still allocate.
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
#include <dlfcn.h>  // NOLINT(misc-include-cleaner)
#include <new>

namespace someip::platform {

static bool g_trap_armed = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void malloc_trap_arm() { g_trap_armed = true; }
void malloc_trap_disarm() { g_trap_armed = false; }
bool malloc_trap_is_armed() { return g_trap_armed; }

}  // namespace someip::platform

namespace {

using MallocFn  = void* (*)(size_t);
using FreeFn    = void (*)(void*);
using CallocFn  = void* (*)(size_t, size_t);
using ReallocFn = void* (*)(void*, size_t);

MallocFn  real_malloc()  { static auto* fn = reinterpret_cast<MallocFn>(dlsym(RTLD_NEXT, "malloc"));   return fn; }  // NOLINT
FreeFn    real_free()    { static auto* fn = reinterpret_cast<FreeFn>(dlsym(RTLD_NEXT, "free"));       return fn; }  // NOLINT
CallocFn  real_calloc()  { static auto* fn = reinterpret_cast<CallocFn>(dlsym(RTLD_NEXT, "calloc"));   return fn; }  // NOLINT
ReallocFn real_realloc() { static auto* fn = reinterpret_cast<ReallocFn>(dlsym(RTLD_NEXT, "realloc")); return fn; }  // NOLINT

}  // namespace

extern "C" {

// NOLINTBEGIN(readability-identifier-naming,cert-dcl58-cpp)

void* malloc(size_t size) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: heap allocation of %zu bytes detected\n", size);
        __builtin_trap();
    }
    return real_malloc()(size);
}

void free(void* ptr) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr, "MALLOC TRAP: free(%p) detected\n", ptr);
        __builtin_trap();
    }
    real_free()(ptr);
}

void* calloc(size_t count, size_t size) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: calloc(%zu, %zu) detected\n", count, size);
        __builtin_trap();
    }
    return real_calloc()(count, size);
}

void* realloc(void* ptr, size_t size) {  // NOLINT(cert-dcl58-cpp)
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: realloc(%p, %zu) detected\n", ptr, size);
        __builtin_trap();
    }
    return real_realloc()(ptr, size);
}

// NOLINTEND(readability-identifier-naming,cert-dcl58-cpp)

}  // extern "C"

// NOLINTBEGIN(cert-dcl58-cpp,misc-new-delete-overloads)

void* operator new(std::size_t size) {
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator new(%zu) detected\n", size);
        __builtin_trap();
    }
    void* p = real_malloc()(size);
    if (!p) { throw std::bad_alloc(); }
    return p;
}

void* operator new[](std::size_t size) {
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator new[](%zu) detected\n", size);
        __builtin_trap();
    }
    void* p = real_malloc()(size);
    if (!p) { throw std::bad_alloc(); }
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator new(%zu, nothrow) detected\n", size);
        __builtin_trap();
    }
    return real_malloc()(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (someip::platform::g_trap_armed) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator new[](%zu, nothrow) detected\n", size);
        __builtin_trap();
    }
    return real_malloc()(size);
}

void operator delete(void* ptr) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr, "MALLOC TRAP: operator delete(%p) detected\n", ptr);
        __builtin_trap();
    }
    real_free()(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator delete[](%p) detected\n", ptr);
        __builtin_trap();
    }
    real_free()(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr, "MALLOC TRAP: operator delete(%p, size) detected\n", ptr);
        __builtin_trap();
    }
    real_free()(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    if (someip::platform::g_trap_armed && ptr) {
        std::fprintf(stderr,
                     "MALLOC TRAP: operator delete[](%p, size) detected\n", ptr);
        __builtin_trap();
    }
    real_free()(ptr);
}

// NOLINTEND(cert-dcl58-cpp,misc-new-delete-overloads)

#endif  // SOMEIP_MALLOC_TRAP
