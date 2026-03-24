<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Cross-Compilation Guide

This guide explains how to build the SOME/IP stack for different embedded targets. The project uses **CMake Presets** as the primary mechanism for reproducible cross-compilation -- each preset encapsulates the toolchain file, platform flags, and build options for a specific target, similar to a "setproduct" approach.

## Concepts

### Presets as "setproduct"

Instead of manually assembling `-D` flags and toolchain file paths, use a preset:

```bash
# This single command selects: ARM toolchain + Cortex-M4 CPU + FreeRTOS PAL + lwIP
cmake --preset freertos-cortexm4
cmake --build --preset freertos-cortexm4
```

Under the hood, the preset in `CMakePresets.json` defines:
- **Toolchain file**: `cmake/toolchains/arm-none-eabi-gcc.cmake`
- **CPU/FPU flags**: `ARM_CPU=cortex-m4`, `ARM_FPU=fpv4-sp-d16`, `ARM_FLOAT_ABI=hard`
- **PAL selection**: `SOMEIP_USE_FREERTOS=ON`, `SOMEIP_USE_LWIP=ON`
- **Build type**: `Release`

### Host presets

For native host builds, use the preset that matches your OS:

```bash
cmake --preset host-linux        # Linux
cmake --preset host-macos        # macOS
cmake --preset host-linux-tests  # Linux with tests
cmake --preset host-macos-tests  # macOS with tests
```

`host-macos` is an alias for `host-linux` — both use the POSIX PAL backend. The separate name avoids confusion when `cmake --list-presets` is run on a Mac.

### Platform Abstraction Layer (PAL)

The PAL is selected via include-path switching at configure time. Each preset sets the appropriate `SOMEIP_USE_*` options, which cause CMake to include the correct `*_impl.h` headers from the platform backend directory. No source code changes are needed to switch targets.

## Prerequisites

### ARM Bare-Metal Targets (FreeRTOS, ThreadX)

Install the ARM GCC toolchain:

```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi

# macOS (Homebrew)
brew install --cask gcc-arm-embedded

# Manual: download from https://developer.arm.com/downloads/-/gnu-rm
```

Verify the installation:

```bash
arm-none-eabi-gcc --version
```

### Zephyr

Zephyr uses its own build system (`west`) and SDK. See the [Zephyr](#zephyr) section below.

## FreeRTOS + lwIP

### Using the preset

```bash
cmake --preset freertos-cortexm4
cmake --build --preset freertos-cortexm4
```

This produces `someip-core.a` (and other library targets) compiled for ARM Cortex-M4 with FreeRTOS threading and lwIP networking.

### Customizing for a different Cortex-M variant

Create a `CMakeUserPresets.json` (git-ignored) to override CPU/FPU settings:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "freertos-cortexm7",
      "displayName": "FreeRTOS ARM Cortex-M7",
      "inherits": "freertos-cortexm4",
      "cacheVariables": {
        "ARM_CPU": "cortex-m7",
        "ARM_FPU": "fpv5-d16"
      }
    }
  ]
}
```

Then:

```bash
cmake --preset freertos-cortexm7
cmake --build build/freertos-cortexm7
```

### Integration with your BSP

The CMake build fetches the **FreeRTOS** or **ThreadX** kernel and **lwIP** (headers) automatically via FetchContent when the corresponding `SOMEIP_USE_*` options are enabled. It also supplies minimal `FreeRTOSConfig.h` and `lwipopts.h` files suitable for CI and smoke builds.

For production firmware, you should use your own RTOS and lwIP trees and tune configuration for your hardware by setting `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL`, `FETCHCONTENT_SOURCE_DIR_THREADX`, or `FETCHCONTENT_SOURCE_DIR_LWIP` so CMake uses your copies instead of the default fetch.

Your board support package (BSP) must still provide:

1. **Linker script** for your specific MCU
2. **Startup code** (reset handler, vector table)
3. **Board-specific configuration** — a production `FreeRTOSConfig.h` and `lwipopts.h` (and ThreadX port/config as required by your BSP) aligned with your hardware and memory map

Link the SOME/IP libraries into your firmware:

```cmake
target_link_libraries(my_firmware PRIVATE someip-core someip-transport someip-sd)
```

### Testing without hardware

#### Linux-port tests (POSIX threads)

Use the Linux-port presets to run the actual FreeRTOS/ThreadX kernel on POSIX threads:

```bash
# FreeRTOS on Linux (POSIX port)
cmake --preset freertos-linux-tests
cmake --build --preset freertos-linux-tests
ctest --preset freertos-linux-tests

# ThreadX on Linux
cmake --preset threadx-linux-tests
cmake --build --preset threadx-linux-tests
ctest --preset threadx-linux-tests
```

These tests exercise the real PAL implementation with real RTOS primitives, without requiring embedded hardware.

#### Renode simulation (real ARM target)

For testing on actual ARM Cortex-M architecture (not POSIX threads), use the Renode presets. These cross-compile full ELF binaries with startup code and UART retarget, then run them on the [Renode](https://renode.io/) open-source hardware simulator:

```bash
# FreeRTOS on Renode (STM32F407 Cortex-M4F)
./scripts/run_freertos_renode_test.sh --timeout 60

# ThreadX on Renode (STM32F407 Cortex-M4)
./scripts/run_threadx_renode_test.sh --timeout 60
```

These scripts handle the full workflow: cross-compile with the Renode preset, launch Renode headless, capture UART output, parse `[PASS]`/`[FAIL]` results, and optionally generate JUnit XML (`--junit-output PATH`).

The Renode presets enable `SOMEIP_USE_LWIP=ON` (headers-only via FetchContent) so the build matches the real deployment configuration. lwIP byteorder macros are resolved inline via `__builtin_bswap` intrinsics defined in `cmake/config/lwip/arch/cc.h`, avoiding a link-time dependency on lwIP's compiled library. A minimal BSP (`bsp/stm32f407_renode/`) provides startup code, a linker script matching the STM32F407 memory map, and UART retarget so `printf` output is captured by Renode's `FileTerminal`.

For Zephyr, the Renode test runner builds and runs tests for the `s32k388_renode` board (NXP S32K388 Cortex-M7):

```bash
# Requires ZEPHYR_BASE to be set
./scripts/run_renode_test.sh test_core --timeout 60
./scripts/run_renode_test.sh test_transport --timeout 60
```

**Prerequisites**: Install [Renode](https://renode.io/download/) and the ARM GCC toolchain (`arm-none-eabi-gcc`).

## ThreadX + lwIP

### Using the preset

```bash
cmake --preset threadx-cortexm4
cmake --build --preset threadx-cortexm4
```

Works identically to the FreeRTOS preset but selects the ThreadX PAL backend. The same customization and BSP integration approach applies.

## Linux Cross-Compilation (with sysroot)

When cross-compiling for *another Linux target* (e.g. building on x86_64 for AArch64 or ARMv7), the target still runs Linux and uses the POSIX PAL — no `SOMEIP_USE_*` flags are needed. You need two things from your vendor or build environment:

1. **A cross-compiler** — e.g. `aarch64-linux-gnu-gcc` / `aarch64-linux-gnu-g++`
2. **A sysroot** — a directory containing the target's C library, kernel headers, and any other libraries. This is typically provided by a Yocto SDK, Buildroot output, or the distro's cross packages.

### Using the preset

```bash
# Install the cross-compiler (Ubuntu/Debian)
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Build opensomeip — pass the sysroot if the compiler doesn't include one
cmake --preset linux-aarch64 -DLINUX_CROSS_SYSROOT=/opt/sysroot/aarch64
cmake --build --preset linux-aarch64
```

If your cross-compiler already ships with a sysroot (common with Yocto/Linaro SDKs), you can omit `-DLINUX_CROSS_SYSROOT` and the compiler's built-in sysroot will be used automatically.

### Customizing for a different target

Override the prefix and architecture for any Linux target. For example, ARMv7 hard-float:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-cross-gcc.cmake \
      -DLINUX_CROSS_PREFIX=arm-linux-gnueabihf- \
      -DLINUX_CROSS_ARCH=arm \
      -DLINUX_CROSS_SYSROOT=/opt/sysroot/armhf \
      -B build/linux-armhf
cmake --build build/linux-armhf
```

Or create a reusable preset in `CMakeUserPresets.json` (git-ignored):

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "linux-armhf",
      "displayName": "Linux ARMv7 Hard-Float",
      "inherits": "linux-cross",
      "cacheVariables": {
        "LINUX_CROSS_ARCH": "arm",
        "LINUX_CROSS_PREFIX": "arm-linux-gnueabihf-",
        "LINUX_CROSS_SYSROOT": "/opt/sysroot/armhf"
      }
    }
  ]
}
```

Then simply: `cmake --preset linux-armhf && cmake --build build/linux-armhf`

### Yocto / Buildroot integration

Most embedded Linux SDKs provide an environment setup script that exports `CC`, `CXX`, `SYSROOT`, etc. You can source that script first and then point the toolchain file at the right paths:

```bash
# Source the Yocto SDK environment
source /opt/poky/environment-setup-aarch64-poky-linux

# Use the SDK's compiler and sysroot
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-cross-gcc.cmake \
      -DLINUX_CROSS_PREFIX="${CROSS_COMPILE}" \
      -DLINUX_CROSS_SYSROOT="${SDKTARGETSYSROOT}" \
      -DLINUX_CROSS_ARCH=aarch64 \
      -B build/yocto-aarch64
cmake --build build/yocto-aarch64
```

### What the sysroot must contain

At a minimum, the sysroot needs:

- **C/C++ standard library** headers and shared objects (glibc, musl, or similar)
- **POSIX threading** (`pthread.h`, `libpthread.so`)
- **BSD sockets** (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`)

These are present in any standard Linux sysroot. No additional embedded libraries (FreeRTOS, lwIP) are required since the target is a full Linux system.

## Zephyr

Zephyr does **not** use CMake Presets. It has its own build system (`west`) and the SOME/IP stack integrates as a Zephyr module.

### Setup

```bash
# Install west
pip install west

# Initialize Zephyr workspace
west init ~/zephyrproject
cd ~/zephyrproject
west update

# Install Zephyr SDK
# See https://docs.zephyrproject.org/latest/develop/getting_started/
```

### Building

Register the SOME/IP stack as a Zephyr extra module and build:

```bash
# native_sim (for testing on the host)
west build -b native_sim zephyr/tests/test_core

# S32K344 (NXP mr_canhubk3 board)
west build -b mr_canhubk3 zephyr/samples/hello_s32k
```

Zephyr's build system automatically selects the Zephyr PAL backend and handles the toolchain via the Zephyr SDK.

### Kconfig options

The module is controlled via Kconfig in `zephyr/Kconfig`:

| Option | Description |
|--------|-------------|
| `CONFIG_SOMEIP` | Enable the SOME/IP stack |
| `CONFIG_SOMEIP_TRANSPORT_UDP` | Enable UDP transport |
| `CONFIG_SOMEIP_TRANSPORT_TCP` | Enable TCP transport |
| `CONFIG_SOMEIP_SD` | Enable Service Discovery |
| `CONFIG_SOMEIP_RPC` | Enable RPC client/server |
| `CONFIG_SOMEIP_EVENTS` | Enable event pub/sub |
| `CONFIG_SOMEIP_E2E` | Enable E2E protection |
| `CONFIG_SOMEIP_TP` | Enable SOME/IP-TP |

## Adding a New Target

To add support for a new board or RTOS:

1. **Create the PAL backend** (if the RTOS is new):
   - Add `include/platform/<rtos>/thread_impl.h` and `memory_impl.h`
   - Add `src/platform/<rtos>/memory.cpp` if needed
   - Add mocks in `tests/mocks/<rtos>/`

2. **Add PAL selection logic** in `CMakeLists.txt` and `src/CMakeLists.txt`

3. **Create a preset** in `CMakePresets.json`:

   ```json
   {
     "name": "myrtos-cortexm4",
     "inherits": "base",
     "toolchainFile": "${sourceDir}/cmake/toolchains/arm-none-eabi-gcc.cmake",
     "cacheVariables": {
       "SOMEIP_USE_MYRTOS": "ON",
       "SOMEIP_USE_LWIP": "ON"
     }
   }
   ```

4. **Add a CI job** in `.github/workflows/` for the new target

5. **Add PAL conformance tests** using the shared `tests/pal_conformance_tests.inc`

## Toolchain Reference

### `cmake/toolchains/arm-none-eabi-gcc.cmake`

Parameterized ARM bare-metal toolchain. All variables are CMake cache variables, overridable from presets or the command line:

| Variable | Default | Description |
|----------|---------|-------------|
| `ARM_CPU` | `cortex-m4` | `-mcpu` value |
| `ARM_FPU` | `fpv4-sp-d16` | `-mfpu` value |
| `ARM_FLOAT_ABI` | `hard` | `-mfloat-abi` value |
| `ARM_TOOLCHAIN_PREFIX` | `arm-none-eabi-` | Toolchain binary prefix |

Compiler flags set by the toolchain:
- `-mthumb` (Thumb instruction set)
- `-ffunction-sections -fdata-sections` (dead code elimination via `--gc-sections`)
- `-fno-exceptions -fno-rtti` (C++ only, for embedded size optimization)
- `-specs=nosys.specs` (bare-metal, no OS syscalls)
