# FreeRTOS + lwIP Platform Port

## Overview

OpenSOME/IP supports FreeRTOS + lwIP as a platform backend via the
platform abstraction layer. This enables the SOME/IP stack to run on
any FreeRTOS-based embedded system with lwIP networking, without
coupling to any specific BSW framework.

## CMake Flags

| Flag | Default | Description |
|------|---------|-------------|
| `SOMEIP_USE_FREERTOS` | `OFF` | Use FreeRTOS for threading primitives (Mutex, Thread, ConditionVariable, sleep_for) |
| `SOMEIP_USE_LWIP` | `OFF` | Use lwIP socket API for networking (TCP/UDP sockets, byte-order) |

Both flags should be enabled together for a complete embedded build:

```cmake
cmake -B build \
  -DSOMEIP_USE_FREERTOS=ON \
  -DSOMEIP_USE_LWIP=ON \
  -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake
```

## Required FreeRTOS Configuration

The following `FreeRTOSConfig.h` settings are required:

| Setting | Value | Why |
|---------|-------|-----|
| `configUSE_MUTEXES` | `1` | `platform::Mutex` uses `xSemaphoreCreateMutex()` |
| `configUSE_COUNTING_SEMAPHORES` | `1` | `platform::ConditionVariable` uses `xSemaphoreCreateCounting()` |
| `configSUPPORT_DYNAMIC_ALLOCATION` | `1` | Task and semaphore creation use dynamic allocation |

## Required lwIP Configuration

The following `lwipopts.h` settings are required:

| Setting | Value | Why |
|---------|-------|-----|
| `LWIP_SOCKET` | `1` | Socket API is the transport layer's interface to the network stack |
| `LWIP_COMPAT_SOCKETS` | `1` (recommended) | When enabled, standard POSIX socket names are available without macro mapping. When `0`, the backend provides explicit macros. |
| `LWIP_PROVIDE_ERRNO` | `1` | Required if the C library does not provide `errno` |
| `LWIP_TCP` | `1` | Required for SOME/IP TCP transport |
| `LWIP_UDP` | `1` | Required for SOME/IP UDP transport and Service Discovery |
| `LWIP_IGMP` | `1` | Required for SOME/IP-SD multicast group management |

### lwIP API Compatibility Notes

- **`poll`/`select`**: Supported via `lwip_poll()`/`lwip_select()`.
- **`fcntl`**: Only `O_NONBLOCK` is meaningful in lwIP.
- **`SO_RCVTIMEO`/`SO_SNDTIMEO`**: Supported with `struct timeval`.
- **TCP keep-alive**: Not supported on lwIP builds; the keep-alive
  block in `tcp_transport.cpp` is compiled out.

## Configurable Defines

| Define | Default | Description |
|--------|---------|-------------|
| `SOMEIP_FREERTOS_THREAD_STACK_SIZE` | `4096` | Stack size in bytes for SOME/IP worker threads |
| `SOMEIP_FREERTOS_THREAD_PRIORITY` | `tskIDLE_PRIORITY + 2` | FreeRTOS task priority for SOME/IP threads |
| `SOMEIP_FREERTOS_MESSAGE_POOL_SIZE` | `16` | Number of pre-allocated Message objects in the static pool |

## Thread Lifecycle

- **Creation**: `xTaskCreate()` with a trampoline function. A binary
  semaphore is created for `join()` synchronization.
- **join()**: Blocks on the binary semaphore until the user function
  completes. Idempotent (second call is a no-op). Cleans up the task
  handle, trampoline context, and join semaphore.
- **Destructor (task still running)**: Aborts the task via
  `vTaskDelete()`, then cleans up all resources. Matches the Zephyr
  backend behavior (`k_thread_abort`).

## ConditionVariable Semantics

FreeRTOS has no native condition variable. The backend uses a counting
semaphore:

- **`notify_one()`**: `xSemaphoreGive()` — one waiter (if any)
  unblocks.
- **`wait(Mutex& mtx)`**: Unlocks the mutex, takes the semaphore
  (blocking), re-locks the mutex.
- **`wait(Mutex& mtx, Pred pred)`**: Predicate loop — `while
  (!pred()) { wait(mtx); }`.

Callers **must** use the predicate form to handle stale tokens.

## Memory Pool

Message objects are allocated from a fixed-size static pool to avoid
heap fragmentation:

- **Alignment**: `alignas(Message)` on the pool buffer.
- **Exhaustion**: `allocate_message()` returns `nullptr`. Callers
  must check the return value.
- **Locking**: One FreeRTOS mutex per pool; all alloc/release
  operations are under that mutex.

## Integrator Example

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_embedded_app LANGUAGES CXX)

set(CMAKE_TOOLCHAIN_FILE "path/to/arm-none-eabi.cmake")

set(SOMEIP_USE_FREERTOS ON CACHE BOOL "")
set(SOMEIP_USE_LWIP     ON CACHE BOOL "")

add_subdirectory(third_party/some-ip someip_build)

# Integrator provides FreeRTOS and lwIP headers + libs
target_include_directories(someip-core      PUBLIC ${FREERTOS_INCLUDE_DIRS})
target_include_directories(someip-transport PUBLIC ${LWIP_INCLUDE_DIRS})

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE someip-transport someip-core freertos_kernel lwip)
```

## CI Runtime Testing (FreeRTOS POSIX Port)

FreeRTOS provides an official POSIX/Linux simulator port
(`portable/ThirdParty/GCC/Posix`) that runs the FreeRTOS kernel as a
native Linux process using pthreads. This enables real FreeRTOS runtime
testing in CI without any emulator or hardware.

```cmake
cmake -B build \
  -DSOMEIP_FREERTOS_LINUX_TESTS=ON
```

This option:
- Fetches FreeRTOS-Kernel V11.1.0 via FetchContent
- Builds FreeRTOS with `FREERTOS_PORT=GCC_POSIX`, `FREERTOS_HEAP=4`
- Enables `SOMEIP_USE_FREERTOS=ON` automatically
- Builds and runs `test_freertos_core`, which boots the real FreeRTOS
  scheduler (`vTaskStartScheduler()`) and exercises Message, Endpoint,
  Serializer, threading primitives (Mutex, ConditionVariable, Thread
  join, sleep_for), and the memory pool -- all inside a FreeRTOS task.

The test task calls `exit()` after completing, since
`vTaskStartScheduler()` never returns.

## Supported Platform Combinations

| Platform | Threading Backend | Networking Backend | Status |
|----------|-------------------|--------------------|--------|
| POSIX (Linux/macOS) | `std::thread` | POSIX sockets | Supported |
| Windows | `std::thread` | Winsock2 | Supported |
| Zephyr | `k_thread` / `k_mutex` | Zephyr zsock | Supported |
| FreeRTOS + lwIP | `xTaskCreate` / `xSemaphore` | lwIP sockets | Supported |
| ThreadX + lwIP | `tx_thread_create` / `TX_MUTEX` | lwIP sockets | Supported |
