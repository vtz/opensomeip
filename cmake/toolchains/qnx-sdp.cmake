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
# QNX Neutrino SDP toolchain (POSIX sockets)
#
# Compiler and sysroot come from the environment a sourced QNX SDP sets:
#   QNX_HOST    host tools, including usr/bin/qcc and usr/bin/q++
#   QNX_TARGET  target sysroot (CMAKE_SYSROOT)
#
# Intended SDP: 8.0.0, x86_64 Neutrino (qcc -Vgcc_ntox86_64), matching
# --config=x86_64-qnx / x86_64-qnx-sdp_8.0.0-posix.
#
# CMAKE_SYSTEM_NAME is QNX. The root CMakeLists.txt selects
# include/platform/posix unless FreeRTOS, ThreadX, or WIN32 is selected.
# This file does not add a PAL, a message-passing transport, or a link
# line. It was not run against a licensed SDP.
#
# Usage, after sourcing the SDP environment (qnxsdp-env.sh):
#   cmake -S . -B build/qnx-x86_64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/qnx-sdp.cmake \
#         -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
#
# If QNX_HOST or QNX_TARGET is unset, configuration stops. There is no
# fallback to the host compiler.
################################################################################

set(_qnx_sdp_missing "")
if(NOT DEFINED ENV{QNX_HOST} OR "$ENV{QNX_HOST}" STREQUAL "")
  string(APPEND _qnx_sdp_missing " QNX_HOST")
endif()
if(NOT DEFINED ENV{QNX_TARGET} OR "$ENV{QNX_TARGET}" STREQUAL "")
  string(APPEND _qnx_sdp_missing " QNX_TARGET")
endif()
if(_qnx_sdp_missing)
  message(FATAL_ERROR
    "QNX SDP environment is incomplete (missing:${_qnx_sdp_missing}). "
    "Source the SDP environment (qnxsdp-env.sh) so QNX_HOST and QNX_TARGET "
    "are set. This toolchain does not fall back to the host compiler.")
endif()

set(QNX_HOST "$ENV{QNX_HOST}")
set(QNX_TARGET "$ENV{QNX_TARGET}")

if(NOT EXISTS "${QNX_HOST}/usr/bin/qcc" OR NOT EXISTS "${QNX_HOST}/usr/bin/q++")
  message(FATAL_ERROR
    "QNX_HOST (${QNX_HOST}) does not contain usr/bin/qcc and usr/bin/q++. "
    "This toolchain does not fall back to the host compiler.")
endif()
if(NOT IS_DIRECTORY "${QNX_TARGET}")
  message(FATAL_ERROR
    "QNX_TARGET (${QNX_TARGET}) is not a directory. "
    "This toolchain does not use a host sysroot.")
endif()

set(CMAKE_SYSTEM_NAME QNX)

# Default matches SDP 8.0.0 x86_64 Neutrino. qcc -V takes the target name
# (gcc_ntox86_64). Do not append "le"; that suffix is for the ARM targets.
set(QNX_SDP_ARCH "gcc_ntox86_64" CACHE STRING
  "qcc -V target passed as CMAKE_<LANG>_COMPILER_TARGET")
set(QNX_SDP_PROCESSOR "x86_64" CACHE STRING
  "CMAKE_SYSTEM_PROCESSOR for the Neutrino target")

set(CMAKE_SYSTEM_PROCESSOR "${QNX_SDP_PROCESSOR}")

# Same qcc -V pattern CMake uses to derive the nto-prefixed binutils names,
# so ar/strip come from QNX_HOST rather than the host.
if(NOT QNX_SDP_ARCH MATCHES "^gcc_nto([a-z0-9]+_[0-9]+|[^_le]+)(le)?$")
  message(FATAL_ERROR
    "QNX_SDP_ARCH '${QNX_SDP_ARCH}' is not a qcc -V target "
    "(expected gcc_ntox86_64, or another gcc_nto* name).")
endif()
set(_qnx_bin_prefix "nto${CMAKE_MATCH_1}-")

set(CMAKE_C_COMPILER "${QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "${QNX_HOST}/usr/bin/q++")
set(CMAKE_C_COMPILER_TARGET "${QNX_SDP_ARCH}")
set(CMAKE_CXX_COMPILER_TARGET "${QNX_SDP_ARCH}")

set(CMAKE_AR "${QNX_HOST}/usr/bin/${_qnx_bin_prefix}ar")
set(CMAKE_RANLIB "${QNX_HOST}/usr/bin/${_qnx_bin_prefix}ranlib")
set(CMAKE_STRIP "${QNX_HOST}/usr/bin/${_qnx_bin_prefix}strip")
set(CMAKE_OBJCOPY "${QNX_HOST}/usr/bin/${_qnx_bin_prefix}objcopy")
set(CMAKE_OBJDUMP "${QNX_HOST}/usr/bin/${_qnx_bin_prefix}objdump")

set(CMAKE_SYSROOT "${QNX_TARGET}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
