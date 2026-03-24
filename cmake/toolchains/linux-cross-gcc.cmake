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
# Linux-to-Linux cross-compilation toolchain
#
# For building on one Linux host (e.g. x86_64) targeting a different Linux
# architecture (e.g. aarch64, armhf) using a cross-compiler and sysroot.
#
# Parameterized via cache variables:
#   LINUX_CROSS_ARCH    - target arch for CMAKE_SYSTEM_PROCESSOR (default: aarch64)
#   LINUX_CROSS_PREFIX  - toolchain binary prefix (default: aarch64-linux-gnu-)
#   LINUX_CROSS_SYSROOT - path to target sysroot (optional, auto-detected if not set)
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-cross-gcc.cmake \
#         -DLINUX_CROSS_SYSROOT=/opt/sysroot/aarch64 ..
#
#   cmake --preset linux-aarch64   (uses this toolchain via CMakePresets.json)
################################################################################

set(CMAKE_SYSTEM_NAME Linux)

set(LINUX_CROSS_ARCH   "aarch64"           CACHE STRING "Target architecture")
set(LINUX_CROSS_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross-compiler binary prefix")

set(CMAKE_SYSTEM_PROCESSOR "${LINUX_CROSS_ARCH}")

set(CMAKE_C_COMPILER   "${LINUX_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${LINUX_CROSS_PREFIX}g++")
set(CMAKE_AR           "${LINUX_CROSS_PREFIX}ar")
set(CMAKE_RANLIB       "${LINUX_CROSS_PREFIX}ranlib")
set(CMAKE_STRIP        "${LINUX_CROSS_PREFIX}strip")
set(CMAKE_OBJCOPY      "${LINUX_CROSS_PREFIX}objcopy")
set(CMAKE_OBJDUMP      "${LINUX_CROSS_PREFIX}objdump")

# Sysroot — set explicitly or let the cross-compiler's built-in sysroot apply.
# When set, CMake passes --sysroot= to every compiler invocation.
set(LINUX_CROSS_SYSROOT "" CACHE PATH
    "Path to the target sysroot (leave empty to use the compiler's default)")

if(LINUX_CROSS_SYSROOT)
  set(CMAKE_SYSROOT "${LINUX_CROSS_SYSROOT}")
endif()

# Search paths: find target libraries/headers in the sysroot, not on the host.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
