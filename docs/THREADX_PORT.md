# ThreadX + lwIP Platform Port

## Overview

OpenSOME/IP supports ThreadX + lwIP as a platform backend via the
platform abstraction layer. This enables the SOME/IP stack to run on
any ThreadX-based embedded system with lwIP networking, without
coupling to any specific BSW framework.

## CMake Flags

| Flag | Default | Description |
|------|---------|-------------|
| `SOMEIP_USE_THREADX` | `OFF` | Use ThreadX for threading primitives (Mutex, Thread, ConditionVariable, sleep_for) |
| `SOMEIP_USE_LWIP` | `OFF` | Use lwIP socket API for networking (TCP/UDP sockets, byte-order) |

Both flags should be enabled together for a complete embedded build:

```cmake
cmake -B build \
  -DSOMEIP_USE_THREADX=ON \
  -DSOMEIP_USE_LWIP=ON \
  -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake
```

## Required ThreadX Configuration

ThreadX requires only a standard `tx_api.h` header.  No special
configuration macros are needed beyond what a typical ThreadX port
provides.  The backend uses:

| API | Purpose |
|-----|---------|
| `tx_mutex_create` / `tx_mutex_get` / `tx_mutex_put` | `platform::Mutex` with `TX_INHERIT` for priority inheritance |
| `tx_event_flags_create` / `tx_event_flags_set` / `tx_event_flags_get` | `platform::ConditionVariable` and `Thread::join()` |
| `tx_thread_create` / `tx_thread_terminate` / `tx_thread_delete` | `platform::Thread` with static stack |
| `tx_thread_sleep` | `platform::this_thread::sleep_for` |
| `tx_block_pool_create` / `tx_block_allocate` / `tx_block_release` | Message pool allocator |

## Required lwIP Configuration

Same as the FreeRTOS port — see [FREERTOS_PORT.md](FREERTOS_PORT.md)
for the full lwIP configuration table.  The key settings are:

| Setting | Value |
|---------|-------|
| `LWIP_SOCKET` | `1` |
| `LWIP_COMPAT_SOCKETS` | `1` (recommended) |
| `LWIP_TCP` | `1` |
| `LWIP_UDP` | `1` |
| `LWIP_IGMP` | `1` |

## Configurable Defines

| Define | Default | Description |
|--------|---------|-------------|
| `SOMEIP_THREADX_THREAD_STACK_SIZE` | `4096` | Stack size in bytes for SOME/IP worker threads (static allocation per Thread object) |
| `SOMEIP_THREADX_THREAD_PRIORITY` | `16` | ThreadX thread priority for SOME/IP threads (lower = higher priority in ThreadX) |
| `SOMEIP_THREADX_MESSAGE_POOL_SIZE` | `16` | Number of pre-allocated Message objects in the TX_BLOCK_POOL |

## Key Differences from FreeRTOS Port

| Aspect | FreeRTOS | ThreadX |
|--------|----------|---------|
| **Mutex** | Binary semaphore (`xSemaphoreCreateMutex`) | `TX_MUTEX` with `TX_INHERIT` (native priority inheritance) |
| **ConditionVariable** | Counting semaphore | `TX_EVENT_FLAGS_GROUP` (flag bit 0) |
| **Thread stack** | Allocated by FreeRTOS kernel | Static `UCHAR stack_[]` member in Thread object |
| **Trampoline signature** | `void(*)(void*)` | `void(*)(ULONG)` |
| **Join mechanism** | Binary semaphore | Event flags group (flag bit 0) |
| **Memory pool** | Manual bitmap + static buffer | `TX_BLOCK_POOL` (native ThreadX block allocator) |
| **sleep_for** | `vTaskDelay(pdMS_TO_TICKS(ms))` | `tx_thread_sleep(ms * TX_TIMER_TICKS_PER_SECOND / 1000)` |

## Thread Lifecycle

- **Creation**: `tx_thread_create()` with a static stack buffer
  (`UCHAR stack_[SOMEIP_THREADX_THREAD_STACK_SIZE]`). A
  `TX_EVENT_FLAGS_GROUP` is created for `join()` synchronization.
  The trampoline casts the `ULONG` parameter back to a `Thread*`.
- **join()**: Blocks on the event flags group via
  `tx_event_flags_get()` until the user function completes.
  Idempotent (second call is a no-op). After join, the thread
  control block is deleted and resources are cleaned up.
- **Destructor (task still running)**: Terminates the task via
  `tx_thread_terminate()`, deletes it via `tx_thread_delete()`,
  then cleans up all resources.

## ConditionVariable Semantics

ThreadX has no native condition variable. The backend uses a
`TX_EVENT_FLAGS_GROUP` with a single flag bit:

- **`notify_one()`**: `tx_event_flags_set(ev, 0x1, TX_OR)` — sets
  the flag. One waiter using `TX_OR_CLEAR` will unblock and clear
  the flag.
- **`wait(Mutex& mtx)`**: Unlocks the mutex, waits for flag bit 0
  with `TX_OR_CLEAR`, re-locks the mutex.
- **`wait(Mutex& mtx, Pred pred)`**: Predicate loop — `while
  (!pred()) { wait(mtx); }`.

Callers **must** use the predicate form to handle races.

## Memory Pool

Message objects are allocated from a `TX_BLOCK_POOL` backed by a
static buffer:

- **Initialization**: Lazy, guarded by a `TX_MUTEX`.
  `tx_block_pool_create()` is called once on first allocation.
- **Allocation**: `tx_block_allocate()` with `TX_NO_WAIT`. Returns
  `nullptr` on pool exhaustion.
- **Release**: Calls the Message destructor, then
  `tx_block_release()`.

## Integrator Example

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_embedded_app LANGUAGES CXX)

set(CMAKE_TOOLCHAIN_FILE "path/to/arm-none-eabi.cmake")

set(SOMEIP_USE_THREADX ON CACHE BOOL "")
set(SOMEIP_USE_LWIP    ON CACHE BOOL "")

add_subdirectory(third_party/some-ip someip_build)

# Integrator provides ThreadX and lwIP headers + libs
target_include_directories(someip-core      PUBLIC ${THREADX_INCLUDE_DIRS})
target_include_directories(someip-transport PUBLIC ${LWIP_INCLUDE_DIRS})

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE someip-transport someip-core threadx lwip)
```

## CI Runtime Testing (ThreadX Linux Port)

Eclipse ThreadX includes an official `linux` port that runs the ThreadX
kernel as a native Linux process (using pthreads underneath). This
enables real ThreadX runtime testing in CI without any emulator or
hardware.

```cmake
cmake -B build \
  -DSOMEIP_THREADX_LINUX_TESTS=ON
```

This option:
- Fetches Eclipse ThreadX v6.4.1 via FetchContent
- Builds ThreadX with `THREADX_ARCH=linux`, `THREADX_TOOLCHAIN=gnu`
- Enables `SOMEIP_USE_THREADX=ON` automatically
- Builds and runs `test_threadx_core`, which boots the real ThreadX
  kernel (`tx_kernel_enter()`) and exercises Message, Endpoint,
  Serializer, threading primitives, and the memory pool — all inside
  a ThreadX thread.

The test binary calls `exit()` after completing, since
`tx_kernel_enter()` never returns.

## Supported Platform Combinations

| Platform | Threading Backend | Networking Backend | Status |
|----------|-------------------|--------------------|--------|
| POSIX (Linux/macOS) | `std::thread` | POSIX sockets | Supported |
| Windows | `std::thread` | Winsock2 | Supported |
| Zephyr | `k_thread` / `k_mutex` | Zephyr zsock | Supported |
| FreeRTOS + lwIP | `xTaskCreate` / `xSemaphore` | lwIP sockets | Supported |
| ThreadX + lwIP | `tx_thread_create` / `TX_MUTEX` | lwIP sockets | Supported |
