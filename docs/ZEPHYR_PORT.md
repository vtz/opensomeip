# Zephyr RTOS Platform Port

## Overview

OpenSOME/IP supports Zephyr RTOS as a platform backend via the
platform abstraction layer. This enables the SOME/IP stack to run on
any Zephyr-supported board with networking, from native_sim for
host-based testing to real hardware such as NXP S32K344/S32K388.

## Build System

Zephyr builds use the Zephyr module system rather than CMake flags.
The module is registered in `zephyr/module.yml` and activated via
Kconfig.

```bash
# native_sim (host-based testing)
west build -b native_sim/native/64 zephyr/tests/test_core

# Cross-compile for NXP S32K344
west build -b mr_canhubk3 zephyr/samples/someip_echo
```

The `ZEPHYR_EXTRA_MODULES` variable must point to the OpenSOME/IP
root before `find_package(Zephyr)`:

```cmake
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
```

## Kconfig Options

All options are under the `SOMEIP` menu (`menuconfig SOMEIP`):

| Option | Default | Description |
|--------|---------|-------------|
| `SOMEIP` | `n` | Enable the SOME/IP protocol stack |
| `SOMEIP_TRANSPORT_UDP` | `y` | Enable UDP transport (requires `NET_SOCKETS`) |
| `SOMEIP_TRANSPORT_TCP` | `y` | Enable TCP transport (requires `NET_SOCKETS`) |
| `SOMEIP_SD` | `y` | Enable Service Discovery (requires UDP) |
| `SOMEIP_RPC` | `y` | Enable RPC client and server |
| `SOMEIP_EVENTS` | `y` | Enable event publisher and subscriber |
| `SOMEIP_E2E` | `y` | Enable E2E protection profiles |
| `SOMEIP_TP` | `y` | Enable SOME/IP-TP segmentation |
| `SOMEIP_THREAD_STACK_SIZE` | `4096` | Stack size for SOME/IP worker threads (512--65536) |
| `SOMEIP_MESSAGE_POOL_SIZE` | `16` | Pre-allocated Message objects in `k_mem_slab` (1--1024) |
| `SOMEIP_MAX_PAYLOAD_SIZE` | `1400` | Maximum SOME/IP payload size in bytes |
| `SOMEIP_MAX_SUBSCRIPTIONS` | `8` | Maximum concurrent event subscriptions |
| `SOMEIP_MAX_PENDING_CALLS` | `8` | Maximum pending RPC calls |

## Required Zephyr Configuration

The following `prj.conf` settings are required:

| Setting | Value | Why |
|---------|-------|-----|
| `CONFIG_CPP` | `y` | C++ language support |
| `CONFIG_STD_CPP17` | `y` | C++17 standard library |
| `CONFIG_REQUIRES_FULL_LIBCPP` | `y` | Complete C++ headers (replaces `NEWLIB_LIBC` in Zephyr 4.x) |
| `CONFIG_NETWORKING` | `y` | Networking subsystem |
| `CONFIG_NET_SOCKETS` | `y` | BSD socket API |
| `CONFIG_NET_IPV4` | `y` | IPv4 support |
| `CONFIG_NET_UDP` | `y` | UDP protocol |
| `CONFIG_NET_TCP` | `y` | TCP protocol (if `SOMEIP_TRANSPORT_TCP`) |
| `CONFIG_HEAP_MEM_POOL_SIZE` | `>=65536` | Heap for STL containers (`std::vector`, `std::string`) |

## Threading Backend

The Zephyr threading backend uses native Zephyr kernel primitives:

| API | Purpose |
|-----|---------|
| `k_mutex_init` / `k_mutex_lock` / `k_mutex_unlock` | `platform::Mutex` with priority inheritance |
| `k_condvar_init` / `k_condvar_signal` / `k_condvar_wait` | `platform::ConditionVariable` (native support) |
| `k_thread_create` / `k_thread_join` / `k_thread_abort` | `platform::Thread` with static stack |
| `k_msleep` | `platform::this_thread::sleep_for` |

**Code Location**: `include/platform/zephyr/thread_impl.h`

### Key differences from other backends

| Aspect | Zephyr | FreeRTOS | ThreadX |
|--------|--------|----------|---------|
| **Mutex** | `k_mutex` (native priority inheritance) | Binary semaphore | `TX_MUTEX` with `TX_INHERIT` |
| **ConditionVariable** | `k_condvar` (native) | Counting semaphore (emulated) | `TX_EVENT_FLAGS_GROUP` (emulated) |
| **Thread stack** | `K_KERNEL_STACK_MEMBER` (static) | Allocated by kernel | Static `UCHAR` array |
| **Trampoline** | `void(*)(void*, void*, void*)` | `void(*)(void*)` | `void(*)(ULONG)` |
| **Join** | `k_thread_join` (native) | Binary semaphore | Event flags group |

## Networking Backend

Zephyr provides its own BSD-compatible socket API (`zsock_*`). Since
`CONFIG_NET_SOCKETS_POSIX_NAMES` was removed in Zephyr 4.3.x, the
backend provides explicit macro mappings:

```c
#define socket(...)  zsock_socket(__VA_ARGS__)
#define bind(...)    zsock_bind(__VA_ARGS__)
#define close(...)   zsock_close(__VA_ARGS__)
// ... (all socket functions)
```

Additional mappings for:
- `inet_addr()` via `zsock_inet_pton` (no native `zsock_inet_addr`)
- `in_addr_t` as `uint32_t`
- `F_GETFL`, `F_SETFL`, `O_NONBLOCK` to `ZVFS_*` constants
- `fd_set`, `FD_ZERO/SET/CLR/ISSET` to `ZSOCK_*` variants

`connect` is intentionally *not* macro-mapped to avoid clashing with
the `ITransport::connect()` virtual method. Transport code calls
`zsock_connect()` directly.

**Code Location**: `include/platform/zephyr/net_impl.h`,
`include/platform/zephyr/byteorder_impl.h`

## Memory Pool

Message objects are allocated from a `k_mem_slab` backed by a static
buffer:

- **Pool size**: Controlled by `CONFIG_SOMEIP_MESSAGE_POOL_SIZE`
- **Allocation**: `k_mem_slab_alloc` with `K_NO_WAIT`
- **Release**: Custom deleter in `shared_ptr` calls destructor then
  `k_mem_slab_free`
- Returns `nullptr` on pool exhaustion

**Code Location**: `include/platform/zephyr/memory_impl.h`,
`src/platform/zephyr_memory.cpp`

## native_sim Testing

The `native_sim` target runs Zephyr as a native Linux process,
enabling host-based testing without hardware or emulation.

```bash
west build -b native_sim/native/64 zephyr/tests/test_core
west build -b native_sim/native/64 zephyr/tests/test_transport
```

On `native_sim`, the build system selects the POSIX platform backend
(`include/platform/posix/`) instead of the Zephyr backend, since
`CONFIG_ARCH_POSIX` is defined. This means `std::thread`/`std::mutex`
and standard `std::make_shared` are used, matching the host build.
The Zephyr backend (`k_thread`, `k_mem_slab`, `zsock_*`) is only
active on real hardware targets.

## CI Integration

Zephyr tests run in the `ghcr.io/zephyrproject-rtos/ci` container:

| Job | Target | Type |
|-----|--------|------|
| `test_core` | `native_sim/native/64` | Build + runtime (26 tests) |
| `test_transport` | `native_sim/native/64` | Build + runtime (3 tests) |
| `someip_echo` | `native_sim/native/64` | Build + runtime |
| `mr_canhubk3` | `mr_canhubk3` | Cross-compile only |

See `.github/workflows/zephyr.yml` for the full workflow.

## Architectural Decisions

### Single codebase (no fork)

The protocol code (~95% of the codebase) is identical across all
platforms. Platform differences are isolated to 4 headers in
`include/platform/`. A fork would duplicate all maintenance effort.

### STL containers kept

`std::vector`, `std::string`, `std::shared_ptr` work on Zephyr with
`CONFIG_STD_CPP17` and `CONFIG_REQUIRES_FULL_LIBCPP`. Heap is
controlled via `CONFIG_HEAP_MEM_POOL_SIZE`.

### std::regex removed

The only STL dependency incompatible with RTOS targets (>100KB RAM).
Replaced by manual IPv4/IPv6 parsing in `endpoint.cpp`.

### Zephyr 4.3.x compatibility

- `CONFIG_NEWLIB_LIBC` replaced by `CONFIG_REQUIRES_FULL_LIBCPP`
- `CONFIG_NET_SOCKETS_POSIX_NAMES` removed; macros in `net_impl.h`
- `CONFIG_NATIVE_APPLICATION` replaced by `CONFIG_ARCH_POSIX`
- `K_THREAD_STACK_MEMBER` replaced by `K_KERNEL_STACK_MEMBER`

## Supported Platform Combinations

| Platform | Threading Backend | Networking Backend | Status |
|----------|-------------------|--------------------|--------|
| POSIX (Linux/macOS) | `std::thread` | POSIX sockets | Supported |
| Windows | `std::thread` | Winsock2 | Supported |
| Zephyr | `k_thread` / `k_mutex` | Zephyr zsock | Supported |
| FreeRTOS + lwIP | `xTaskCreate` / `xSemaphore` | lwIP sockets | Supported |
| ThreadX + lwIP | `tx_thread_create` / `TX_MUTEX` | lwIP sockets | Supported |
