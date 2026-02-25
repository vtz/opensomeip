# SOME/IP Stack -- Zephyr RTOS Port Progress

## Project Summary

- **Branch**: `feature/zephyr-port`
- **Targets**: native_sim, mr_canhubk3 (S32K344), s32k388_renode (S32K388)
- **Overall status**: Implementation complete -- awaiting validation in Docker/Zephyr

## Architectural Decisions

### 1. Single codebase vs fork

**Decision**: Keep a single codebase. Do not create an "opensomeip_lite" fork.

**Rationale**: The differences between host and Zephyr are concentrated in 4 platform
abstraction headers (`include/platform/`). The protocol code (~95% of the codebase)
is identical across all platforms. A fork would duplicate all maintenance effort.

### 2. STL containers

**Decision**: Keep `std::vector`, `std::string`, `std::shared_ptr`.

**Rationale**: They work on Zephyr with `CONFIG_STD_CPP17` and `CONFIG_REQUIRES_FULL_LIBCPP`.
Heap is controlled via `CONFIG_HEAP_MEM_POOL_SIZE`.

> **Note (Zephyr 4.3.x)**: `CONFIG_NEWLIB_LIBC` has been replaced by
> `CONFIG_REQUIRES_FULL_LIBCPP=y`. On `native_sim`, `NEWLIB_LIBC` conflicts with
> `EXTERNAL_LIBC` (Kconfig warnings are fatal in 4.3.x).

**Discarded alternatives**: iceoryx_hoofs (does not support Zephyr), custom containers
(premature).

### 3. std::regex

**Decision**: Removed. Replaced by manual parsing in `endpoint.cpp`.

**Rationale**: The only STL dependency incompatible with RTOS (>100KB RAM). Used
only for IPv4/IPv6 validation. Manual parsing is more efficient on all platforms.

### 4. Platform abstraction

**Decision**: 4 minimal headers in `include/platform/`.

- `byteorder.h` -- portable htons/ntohs (someip_htons, etc.)
- `net.h` -- portable sockets (POSIX vs Zephyr BSD sockets)
- `thread.h` -- portable threading (std::thread vs k_thread)
- `memory.h` -- pool allocator for Message objects

---

## Phase 0: Environment Setup

### Status: done

### Decisions made

- Docker based on `ghcr.io/zephyrproject-rtos/ci:v0.27.4`
- Renode 1.15.3 pre-installed in the container
- Branch `feature/zephyr-port` created from `ci/skip-tests-on-docs-only`

### What was done

- Created `Dockerfile.zephyr` with Zephyr SDK + ARM toolchain + Renode
- Created `docker-compose.zephyr.yml` with volume mount and NET_ADMIN
- Created `zephyr/samples/net_test/` -- UDP echo for native_sim
- Created `zephyr/samples/hello_s32k/` -- hello world for mr_canhubk3
- Created `scripts/zephyr_build.sh` -- build helper
- Created `docs/ZEPHYR_PORT_PROGRESS.md` (this file)

### State at end of phase

- Host build: pass (11/11 tests)
- native_sim: infrastructure created (requires Docker to run)
- mr_canhubk3: infrastructure created (requires Docker to run)

---

## Phase 1: Zephyr Module Integration

### Status: done

### What was done

- Created `zephyr/module.yml` -- module registration
- Created `zephyr/CMakeLists.txt` -- conditional build via Kconfig
- Created `zephyr/Kconfig` -- 12 configuration options
- Created `zephyr/prj.conf` -- base config (C++17, POSIX, networking)
- Created `zephyr/boards/native_sim.conf` -- heap 256KB, pool 32
- Created `zephyr/boards/mr_canhubk3.conf` -- heap 64KB, pool 8, no TCP/E2E
- Created `include/platform/byteorder.h` -- someip_htons/ntohs/htonl/ntohl

### Decisions made

- Kconfig entries for each module (SD, RPC, Events, E2E, TP) allow disabling
  features on targets with limited RAM
- `byteorder.h` uses macros (`someip_htons`) instead of inline functions for zero
  overhead on all platforms

---

## Phase 2: Remove std::regex

### Status: done

### What was done

- `src/transport/endpoint.cpp`: `is_valid_ipv4()` rewritten with manual parsing
  (split by '.', validate octets 0-255, reject leading zeros)
- `src/transport/endpoint.cpp`: `is_valid_ipv6()` rewritten with manual validation
  (group counting, hex character validation, '::' support)
- Removed `#include <regex>` from `endpoint.cpp`

### Errors found and resolutions

- None. All 11 host tests passed after the change.

### State at end of phase

- Host build: pass (11/11 tests, including EndpointTest)

---

## Phase 3: Socket Abstraction Layer

### Status: done

### What was done

- Created `include/platform/net.h` -- includes network headers per platform
  (__ZEPHYR__, _WIN32, POSIX)
- Defined helpers: `someip_close_socket()`, `someip_set_nonblocking()`,
  `someip_set_blocking()`
- `include/transport/udp_transport.h`: replaced `#include <netinet/in.h>` with
  `#include "platform/net.h"`
- `src/transport/udp_transport.cpp`: removed direct POSIX includes, replaced
  `close()` with `someip_close_socket()`, `fcntl()` with `someip_set_nonblocking()`
- `src/transport/tcp_transport.cpp`: same replacements
- `src/someip/message.cpp`: replaced `#ifdef _WIN32` block with
  `#include "platform/byteorder.h"`, all `htonl/ntohl` calls with `someip_*`
- `src/serialization/serializer.cpp`: same
- `src/sd/sd_message.cpp`: same
- `src/sd/sd_server.cpp`: same
- `src/e2e/e2e_header.cpp`: same
- `src/e2e/e2e_profiles/standard_profile.cpp`: same
- `tests/test_sd.cpp`: same

### Zephyr 4.3.x Compatibility (updated)

`CONFIG_NET_SOCKETS_POSIX_NAMES` was removed in Zephyr 4.3.x. This Kconfig mapped
POSIX names (`socket`, `bind`, `close`, `fcntl`, etc.) to Zephyr's `zsock_*` functions.
Without it, `<zephyr/net/socket.h>` only exposes functions with the `zsock_` prefix.

**Solution**: `platform/net.h` defines macros that replicate the behavior of the old
`CONFIG_NET_SOCKETS_POSIX_NAMES`:

```c
#define socket(...)     zsock_socket(__VA_ARGS__)
#define bind(...)       zsock_bind(__VA_ARGS__)
#define close(...)      zsock_close(__VA_ARGS__)
#define setsockopt(...) zsock_setsockopt(__VA_ARGS__)
// ... (all socket functions)
```

Additionally:
- `inet_addr()` implemented via `zsock_inet_pton` (`zsock_inet_addr` does not exist)
- `in_addr_t` defined as `uint32_t`
- `INADDR_NONE` defined as `0xffffffff`
- Constants `F_GETFL`, `F_SETFL`, `O_NONBLOCK`, `SHUT_RDWR` mapped to `ZSOCK_*`
- `fd_set`, `timeval`, `FD_ZERO/SET/CLR/ISSET` mapped to `zsock_*`/`ZSOCK_*`

Also fixed:
- `udp_transport.cpp`: `throw std::invalid_argument` guarded with
  `__cpp_exceptions || __EXCEPTIONS` (Zephyr uses `-fno-exceptions` by default)
- `tcp_transport.cpp`: `try/catch` removed (empty `catch` blocks)
- Added `#include <cstddef>` in 6 headers for explicit `size_t` declaration
- Replaced `dynamic_cast` with `static_cast` in `sd_server.cpp` and `sd_client.cpp`
  (Zephyr uses `-fno-rtti`)
- Replaced `CONFIG_NATIVE_APPLICATION` with `CONFIG_ARCH_POSIX` in 7 files
  (deprecated in Zephyr 4.x)

### Modified files (11 .cpp + 1 .h)

| File | Change |
|------|--------|
| `include/transport/udp_transport.h` | `netinet/in.h` -> `platform/net.h` |
| `src/transport/udp_transport.cpp` | POSIX includes -> `platform/net.h`, close -> someip_close_socket |
| `src/transport/tcp_transport.cpp` | POSIX includes -> `platform/net.h`, close/fcntl -> helpers |
| `src/someip/message.cpp` | `arpa/inet.h` -> `platform/byteorder.h`, htonl -> someip_htonl |
| `src/serialization/serializer.cpp` | same |
| `src/sd/sd_message.cpp` | same |
| `src/sd/sd_server.cpp` | same |
| `src/e2e/e2e_header.cpp` | same |
| `src/e2e/e2e_profiles/standard_profile.cpp` | same |
| `tests/test_sd.cpp` | `arpa/inet.h` -> `platform/byteorder.h` |

### State at end of phase

- Host build: pass (11/11 tests)
- Zero direct includes of `arpa/inet.h`, `netinet/*.h`, `sys/socket.h`, `unistd.h`,
  `fcntl.h` in sources (only inside `platform/*.h` with guards)

---

## Phase 4: Portable Threading

### Status: done

### What was done

- Created `include/platform/thread.h`:
  - Host/native_sim: aliases for std::thread, std::mutex, std::condition_variable
  - Embedded Zephyr: wrappers for k_thread, k_mutex, k_condvar with compatible API
  - `platform::this_thread::sleep_for()` portable (k_msleep on embedded)
  - `platform::ScopedLock` compatible with both
- Created `src/platform/zephyr_thread.cpp` (stub, implementation is inline in header)
- Replaced `std::thread`/`std::mutex`/`std::condition_variable` in **19 files**:
  - 6 headers: `udp_transport.h`, `tcp_transport.h`, `session_manager.h`,
    `e2e_profile_registry.h`, `tp_reassembler.h`, `tp_manager.h`
  - 13 sources: `udp_transport.cpp`, `tcp_transport.cpp`, `session_manager.cpp`,
    `sd_server.cpp`, `sd_client.cpp`, `rpc_client.cpp`, `rpc_server.cpp`,
    `event_publisher.cpp`, `event_subscriber.cpp`, `e2e_profile_registry.cpp`,
    `standard_profile.cpp`, `tp_reassembler.cpp`, `tp_manager.cpp`
- Replaced `std::scoped_lock` -> `platform::ScopedLock` in all sources
- Replaced `std::lock_guard<std::mutex>` -> `platform::ScopedLock` in e2e/sd
- Replaced `std::this_thread::sleep_for` -> `platform::this_thread::sleep_for`
- Replaced `std::future`/`std::promise` in `rpc_client.cpp` with `std::atomic` +
  `platform::Mutex` + polling with `platform::this_thread::sleep_for`
- Removed all `#include <mutex>`, `#include <thread>`, `#include <condition_variable>`,
  `#include <future>` from sources (included transitively via `platform/thread.h`)

### Decisions made

- `std::atomic` kept (works on all platforms, no abstraction needed)
- `std::future`/`std::promise` eliminated entirely (libstdc++ dependency
  not available on all embedded targets). Replaced with polling with
  1ms sleep, acceptable for RPC sync calls with much higher network latency.
- `platform::ConditionVariable` does not need `wait()` in the current codebase (CVs are
  used only for `notify_one()`), simplifying the embedded API.

### State at end of phase

- Host build: pass (11/11 tests pass)
- Zero uses of `std::thread`/`std::mutex`/`std::condition_variable` outside `platform/thread.h`

---

## Phase 5: Embedded Memory Management

### Status: done

### What was done

- Created `include/platform/memory.h`:
  - Host: `allocate_message()` -> `std::make_shared<Message>()`
  - Embedded: pool via `k_mem_slab` with `CONFIG_SOMEIP_MESSAGE_POOL_SIZE` slots
- Created `src/platform/zephyr_memory.cpp`:
  - Static buffer sized by Kconfig
  - Custom deleter in shared_ptr to return to slab
- Kconfig entries added in `zephyr/Kconfig`:
  - `SOMEIP_MESSAGE_POOL_SIZE`, `SOMEIP_MAX_PAYLOAD_SIZE`
  - `SOMEIP_MAX_SUBSCRIPTIONS`, `SOMEIP_MAX_PENDING_CALLS`
  - `SOMEIP_THREAD_STACK_SIZE`

---

## Phase 6: S32K388 Board Definition + Renode

### Status: done

### What was done

- Created `zephyr/boards/s32k388_renode/`:
  - `board.yml` -- metadata
  - `s32k388_renode.dts` -- Cortex-M7, 512KB SRAM, 8MB flash, GMAC, LPUART
  - `s32k388_renode_defconfig` -- base configuration
  - `s32k388_renode.conf` -- SOME/IP config (heap 128KB, pool 16)
- Created `zephyr/renode/s32k388_someip.resc` -- Renode script with GMAC + TAP
- Created `scripts/run_renode_test.sh` -- build + execution on Renode

---

## Phase 7: High-Level Module Porting

### Status: done

### What was done

- `zephyr/CMakeLists.txt` already includes SD, RPC, Events conditionally via Kconfig
- All SD/RPC/Events sources already use `platform/net.h` transitively
  (via `transport/udp_transport.h`)
- No additional source modifications needed

---

## Phase 8: Tests and Demonstration

### Status: done

### What was done

- Created `zephyr/samples/someip_echo/` -- SOME/IP demo (serialize, deserialize, endpoint)
- Created `zephyr/tests/test_core/` -- tests for Message, Endpoint, SessionManager, Serializer
- Created `zephyr/tests/test_transport/` -- UDP loopback test
- Created `scripts/run_zephyr_tests.sh` -- orchestrates build + execution for each target

---

## Phase 9: CI and Pull Request

### Status: done

### What was done

- Created `.github/workflows/zephyr.yml` with 3 jobs:
  1. `host-build` -- cmake build + ctest (ubuntu-latest)
  2. `zephyr-native-sim` -- build + runtime (container zephyrproject-rtos/ci)
  3. `zephyr-s32k344-build` -- cross-compile mr_canhubk3 (container)

> **Note**: Job `zephyr-s32k388-renode` removed -- SoC `s32k388` is not available
> in the Zephyr upstream tree. The custom board can be reintroduced when
> SoC support becomes available.

### native_sim Builds (CI)

| Build | Command | Type |
|-------|---------|------|
| test_core | `west build -b native_sim zephyr/tests/test_core` | Build + run |
| test_transport | `west build -b native_sim zephyr/tests/test_transport` | Build + run |
| someip_echo | `west build -b native_sim zephyr/samples/someip_echo` | Build + run |
| someip_sd_client | `west build -b native_sim zephyr/samples/someip_sd_client` | Build only |

### Fixes for Zephyr 4.3.x (container `ci:v0.27.4`)

- Explicit Zephyr SDK v0.17.0 installation (`setup.sh -c` to register CMake packages)
- `ZEPHYR_EXTRA_MODULES` defined BEFORE `find_package(Zephyr)` in all
  test/sample CMakeLists.txt files
- `CONFIG_NEWLIB_LIBC=y` removed (conflicts with `EXTERNAL_LIBC` on `native_sim`)
- `CONFIG_REQUIRES_FULL_LIBCPP=y` added (ensures complete C++ headers)
- `CONFIG_NET_SOCKETS_POSIX_NAMES=y` removed (undefined in Zephyr 4.3.x)
- POSIX-to-zsock macros added in `platform/net.h`
- `pip install jsonschema` added to the init step

### State at end of phase

- Host build: pass (11/11 tests)
- CI workflow: ready for validation on push/PR
