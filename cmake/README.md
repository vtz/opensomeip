# CMake Build System

CMake-based build system for the SOME/IP stack, supporting host (POSIX/Win32) and embedded (FreeRTOS, ThreadX, Zephyr) targets via CMake Presets and cross-compilation toolchain files.

## Quick Start

```bash
# Host build (Linux/macOS, POSIX)
cmake --preset host-linux
cmake --build --preset host-linux

# Host build with tests
cmake --preset host-linux-tests
cmake --build --preset host-linux-tests
ctest --preset host-linux-tests

# Cross-compile for ARM Cortex-M4 with FreeRTOS
cmake --preset freertos-cortexm4
cmake --build --preset freertos-cortexm4

# Cross-compile for ARM Cortex-M4 with ThreadX
cmake --preset threadx-cortexm4
cmake --build --preset threadx-cortexm4
```

List all available presets:

```bash
cmake --list-presets
```

## Available Presets

| Preset | Description | Toolchain |
|--------|-------------|-----------|
| `host-linux` | Native POSIX build (Release) | System compiler |
| `host-linux-tests` | Native POSIX with tests and examples | System compiler |
| `freertos-stub-compile` | FreeRTOS+lwIP stub compile check | System compiler |
| `freertos-linux-tests` | FreeRTOS POSIX port runtime tests | System compiler |
| `freertos-cortexm4` | ARM Cortex-M4 cross-compile (FreeRTOS+lwIP) | `arm-none-eabi-gcc` |
| `threadx-stub-compile` | ThreadX+lwIP stub compile check | System compiler |
| `threadx-linux-tests` | ThreadX Linux port runtime tests | System compiler |
| `threadx-cortexm4` | ARM Cortex-M4 cross-compile (ThreadX+lwIP) | `arm-none-eabi-gcc` |

## Platform Options

| Option | Default | Description |
|--------|---------|-------------|
| `SOMEIP_USE_FREERTOS` | `OFF` | Use FreeRTOS PAL (threading, memory) |
| `SOMEIP_USE_THREADX` | `OFF` | Use ThreadX PAL (threading, memory) |
| `SOMEIP_USE_LWIP` | `OFF` | Use lwIP PAL (networking, byte order) |
| `SOMEIP_FREERTOS_LINUX_TESTS` | `OFF` | FreeRTOS POSIX port runtime tests (Linux only) |
| `SOMEIP_THREADX_LINUX_TESTS` | `OFF` | ThreadX Linux port runtime tests (Linux only) |

When none of the RTOS options are set, the build defaults to POSIX (Linux/macOS) or Win32 (Windows).

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `ON` | Build unit tests (requires Google Test, fetched via FetchContent) |
| `BUILD_EXAMPLES` | `ON` | Build example programs |
| `SAFETY_LEVEL` | `ASIL_B` | Safety level configuration (`ASIL_A` through `ASIL_D`, or `NONE`) |
| `ENABLE_WERROR` | `OFF` | Treat compiler warnings as errors |
| `COVERAGE` | `OFF` | Enable code coverage instrumentation |

## Toolchain Files

### `cmake/toolchains/arm-none-eabi-gcc.cmake`

Generic ARM bare-metal toolchain for Cortex-M targets. Parameterized via cache variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `ARM_CPU` | `cortex-m4` | CPU variant (`cortex-m4`, `cortex-m7`, `cortex-m33`, ...) |
| `ARM_FPU` | `fpv4-sp-d16` | FPU variant (`fpv4-sp-d16`, `fpv5-d16`, ...) |
| `ARM_FLOAT_ABI` | `hard` | Float ABI (`soft`, `softfp`, `hard`) |
| `ARM_TOOLCHAIN_PREFIX` | `arm-none-eabi-` | Toolchain binary prefix |

Override in a preset or on the command line:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake \
      -DARM_CPU=cortex-m7 -DARM_FPU=fpv5-d16 \
      -DSOMEIP_USE_FREERTOS=ON -DSOMEIP_USE_LWIP=ON ..
```

## Zephyr

Zephyr uses its own build system (`west`) and does **not** use CMake Presets. The project integrates as a Zephyr module via `zephyr/module.yml`. See [`docs/cross-compilation.md`](../docs/cross-compilation.md) for Zephyr build instructions.

## Platform Abstraction Layer (PAL)

The build system selects the correct PAL backend via include-path switching. Each platform provides `thread_impl.h`, `memory_impl.h`, `net_impl.h`, and `byteorder_impl.h` under `include/platform/<backend>/`. The dispatching headers in `include/platform/` include the appropriate `*_impl.h` based on which platform directory is on the include path.

| Backend | Threading | Networking | When Selected |
|---------|-----------|------------|---------------|
| `posix` | `std::thread`, `std::mutex` | BSD sockets | Default (Linux/macOS) |
| `win32` | `std::thread`, `std::mutex` | Winsock2 | `WIN32` detected |
| `freertos` | FreeRTOS tasks, semaphores | (via lwIP) | `SOMEIP_USE_FREERTOS=ON` |
| `threadx` | ThreadX threads, mutexes | (via lwIP) | `SOMEIP_USE_THREADX=ON` |
| `zephyr` | Zephyr `k_thread`, `k_mutex` | Zephyr sockets | Zephyr module build |
| `lwip` | -- | lwIP sockets | `SOMEIP_USE_LWIP=ON` |

## Library Targets

| Target | Description |
|--------|-------------|
| `someip-core` | Core protocol + E2E protection |
| `someip-transport` | UDP and TCP transport |
| `someip-sd` | Service Discovery |
| `someip-rpc` | RPC client/server |
| `someip-events` | Event publisher/subscriber |
| `someip-tp` | SOME/IP-TP segmentation/reassembly |
| `someip-serialization` | Payload serialization |

## Dependencies

- **CMake 3.20+** (3.21+ for CMakePresets.json)
- **C++17 compiler**: GCC 9+, Clang 10+, MSVC 2019+
- **Google Test**: Fetched via FetchContent (when `BUILD_TESTS=ON`)
- **arm-none-eabi-gcc**: Required for ARM cross-compilation presets
