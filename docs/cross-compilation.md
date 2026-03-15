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

### Platform Abstraction Layer (PAL)

The PAL is selected via include-path switching at configure time. Each preset sets the appropriate `SOMEIP_USE_*` options, which cause CMake to include the correct `*_impl.h` headers from the platform backend directory. No source code changes are needed to switch targets.

## Prerequisites

### ARM Bare-Metal Targets (FreeRTOS, ThreadX)

Install the ARM GCC toolchain:

```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi g++-arm-none-eabi libnewlib-arm-none-eabi

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

The cross-compiled libraries are standalone -- they do not include FreeRTOS or lwIP themselves. Your board support package (BSP) must provide:

1. **FreeRTOS kernel** headers and library (with a `FreeRTOSConfig.h` for your board)
2. **lwIP** headers and library (with `lwipopts.h` for your board)
3. **Linker script** for your specific MCU
4. **Startup code** (reset handler, vector table)

Link the SOME/IP libraries into your firmware:

```cmake
target_link_libraries(my_firmware PRIVATE someip-core someip-transport someip-sd)
```

### Testing without hardware

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

## ThreadX + lwIP

### Using the preset

```bash
cmake --preset threadx-cortexm4
cmake --build --preset threadx-cortexm4
```

Works identically to the FreeRTOS preset but selects the ThreadX PAL backend. The same customization and BSP integration approach applies.

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
   - Add stubs in `tests/stubs/<rtos>/` and mocks in `tests/mocks/<rtos>/`

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
