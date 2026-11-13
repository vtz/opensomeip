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
# Lock the QNX_SDP_ARCH regex in cmake/toolchains/qnx-sdp.cmake.
#
# The toolchain derives the binutils prefix from CMAKE_MATCH_1:
#   ntox86_64-ar  for gcc_ntox86_64
#   ntoaarch64-ar for gcc_ntoaarch64le
#   ntoarmv7-ar   for gcc_ntoarmv7le
#
# Run: cmake -P tests/qnx-sdp-arch-regex.cmake
################################################################################

cmake_minimum_required(VERSION 3.20)

# Bracket form so the trailing '$' is not expanded. This is the MATCHES
# pattern in qnx-sdp.cmake; the file check below rejects a drifted copy.
set(QNX_SDP_ARCH_REGEX [[^gcc_nto([a-z0-9]+_[0-9]+|[^_le]+)(le)?$]])

set(_qnx_toolchain "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchains/qnx-sdp.cmake")
if(NOT EXISTS "${_qnx_toolchain}")
  message(FATAL_ERROR "toolchain file not found: ${_qnx_toolchain}")
endif()
file(READ "${_qnx_toolchain}" _qnx_toolchain_text)
string(FIND "${_qnx_toolchain_text}" "MATCHES \"${QNX_SDP_ARCH_REGEX}\"" _qnx_regex_at)
if(_qnx_regex_at EQUAL -1)
  message(FATAL_ERROR
    "cmake/toolchains/qnx-sdp.cmake does not contain "
    "MATCHES \"${QNX_SDP_ARCH_REGEX}\". "
    "Update this test to the regex the toolchain actually uses.")
endif()

# if(MATCHES) is what the toolchain uses. Copy CMAKE_MATCH_1 inside the
# true branch, before a later if() can restore the previous match state.
function(expect_arch_match arch expected_group)
  if("${arch}" MATCHES "${QNX_SDP_ARCH_REGEX}")
    set(captured "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR
      "QNX_SDP_ARCH '${arch}' did not match the toolchain regex")
  endif()
  if(NOT captured STREQUAL expected_group)
    message(FATAL_ERROR
      "QNX_SDP_ARCH '${arch}' CMAKE_MATCH_1='${captured}' "
      "expected '${expected_group}' "
      "(binutils prefix nto${captured}-)")
  endif()
  message(STATUS "${arch} -> nto${captured}-")
endfunction()

expect_arch_match("gcc_ntox86_64" "x86_64")
expect_arch_match("gcc_ntoaarch64le" "aarch64")
# 'le' is the optional suffix. The prefix group is armv7, so the
# toolchain's binutils name is ntoarmv7-ar, not ntoarmv7le-ar.
expect_arch_match("gcc_ntoarmv7le" "armv7")

if("not_a_target" MATCHES "${QNX_SDP_ARCH_REGEX}")
  message(FATAL_ERROR
    "not_a_target matched the toolchain regex "
    "(CMAKE_MATCH_1='${CMAKE_MATCH_1}')")
endif()

message(STATUS "QNX_SDP_ARCH regex checks passed")
