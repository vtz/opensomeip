################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

################################################################################
# ARM bare-metal cross-compilation toolchain (arm-none-eabi-gcc)
#
# Parameterized via cache variables so CMakePresets.json can override per-target:
#   ARM_CPU        - e.g. cortex-m4, cortex-m7, cortex-m33  (default: cortex-m4)
#   ARM_FPU        - e.g. fpv4-sp-d16, fpv5-d16             (default: fpv4-sp-d16)
#   ARM_FLOAT_ABI  - soft, softfp, hard                     (default: hard)
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake ..
#   cmake --preset freertos-cortexm4   (uses this toolchain via CMakePresets.json)
################################################################################

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain binaries — override ARM_TOOLCHAIN_PREFIX if the tools are not on
# PATH or have a non-standard prefix.
set(ARM_TOOLCHAIN_PREFIX "arm-none-eabi-" CACHE STRING
    "Prefix for ARM toolchain binaries")

set(CMAKE_C_COMPILER   "${ARM_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${ARM_TOOLCHAIN_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${ARM_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR           "${ARM_TOOLCHAIN_PREFIX}ar")
set(CMAKE_OBJCOPY      "${ARM_TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_OBJDUMP      "${ARM_TOOLCHAIN_PREFIX}objdump")
set(CMAKE_SIZE         "${ARM_TOOLCHAIN_PREFIX}size")

# CPU / FPU configuration — sensible defaults for Cortex-M4F
set(ARM_CPU       "cortex-m4"    CACHE STRING "ARM CPU variant (e.g. cortex-m4, cortex-m7)")
set(ARM_FPU       "fpv4-sp-d16"  CACHE STRING "ARM FPU variant (e.g. fpv4-sp-d16, fpv5-d16)")
set(ARM_FLOAT_ABI "hard"         CACHE STRING "ARM float ABI (soft, softfp, hard)")

set(ARM_COMMON_FLAGS "-mcpu=${ARM_CPU} -mthumb -mfpu=${ARM_FPU} -mfloat-abi=${ARM_FLOAT_ABI}")

set(CMAKE_C_FLAGS_INIT   "${ARM_COMMON_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${ARM_COMMON_FLAGS} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "${ARM_COMMON_FLAGS}")

# -specs=nosys.specs provides syscall stubs (-lnosys) for bare-metal.
# It must appear ONLY in the linker flags, not in C/CXX flags, to avoid a
# Debian/Ubuntu packaging bug where loading the spec file twice triggers
# "attempt to rename spec 'link_gcc_c_sequence' to already defined spec".
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -specs=nosys.specs")

# Bare-metal: skip compiler introspection that requires running code on the host
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search only in the toolchain sysroot, not on the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
