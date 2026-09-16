# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(opensomeip_cmake_package_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(opensomeip REQUIRED)

add_executable(opensomeip_cmake_package_smoke smoke.cpp)
target_link_libraries(opensomeip_cmake_package_smoke PRIVATE opensomeip::opensomeip)
