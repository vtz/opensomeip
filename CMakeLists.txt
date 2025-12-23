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

cmake_minimum_required(VERSION 3.20)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

project(someip-stack VERSION ${PROJECT_VERSION} LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include FetchContent for dependencies
include(FetchContent)

# Safety-critical build options
option(SAFETY_LEVEL "Safety level (ASIL_A, ASIL_B, ASIL_C, ASIL_D)" "ASIL_B")
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_VSOMEIP_INTEROP "Build vsomeip interoperability examples (requires vsomeip3 and Boost)" OFF)
option(BUILD_TOOLS "Build development tools (reserved for future use)" OFF)
option(COVERAGE "Enable code coverage reporting" OFF)

# Fetch Google Test
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
    )

    # Prevent overriding the parent project's compiler/linker on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    # Set GTest variables for compatibility
    set(GTEST_LIBRARIES gtest gtest_main)
    set(GTEST_INCLUDE_DIRS ${googletest_SOURCE_DIR}/include)
endif()

# Compiler flags for safety
if(SAFETY_LEVEL STREQUAL "ASIL_B" OR SAFETY_LEVEL STREQUAL "ASIL_C" OR SAFETY_LEVEL STREQUAL "ASIL_D")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
endif()

# Code coverage
if(COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -O0")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
    endif()
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(include)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static analysis and formatting
find_program(CLANG_TIDY_EXE clang-tidy
    PATHS /opt/homebrew/Cellar/llvm/*/bin
          /usr/local/opt/llvm/bin
          /usr/local/bin
          /usr/bin
)
find_program(CLANG_FORMAT_EXE clang-format
    PATHS /opt/homebrew/Cellar/llvm/*/bin
          /usr/local/opt/llvm/bin
          /usr/local/bin
          /usr/bin
)

# Disable compilation-time clang-tidy to avoid build warnings
# Use 'make tidy' target for manual static analysis instead
# if(CLANG_TIDY_EXE)
#     set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE}
#         --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
#         --header-filter=.*)
# endif()

# Add custom targets for code quality
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND find ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/examples ${CMAKE_SOURCE_DIR}/tests
               -name "*.cpp" -o -name "*.h" |
               xargs ${CLANG_FORMAT_EXE} -i
        COMMENT "Formatting code with clang-format"
    )
endif()

if(CLANG_TIDY_EXE)
    # Check if test targets exist (indicates successful googletest setup)
    if(TARGET test_message)
        add_custom_target(tidy
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_clang_tidy.sh
                    ${CLANG_TIDY_EXE}
                    ${CMAKE_SOURCE_DIR}/.clang-tidy
                    ${CMAKE_BINARY_DIR}
                    ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy static analysis on all source files"
        )
    else()
        add_custom_target(tidy
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_clang_tidy.sh
                    ${CLANG_TIDY_EXE}
                    ${CMAKE_SOURCE_DIR}/.clang-tidy
                    ${CMAKE_BINARY_DIR}
                    ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy static analysis on all source files"
        )
        message(STATUS "googletest not available - excluding test files from clang-tidy analysis")
    endif()
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found. Run ./scripts/install_dev_tools.sh to install development tools."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-tidy not available - install development tools first"
    )
endif()

# Subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Note: Tools are scripts/utilities and don't require CMake building
# BUILD_TOOLS option is kept for future CMake-based tools

# Note: Coverage reporting is handled by the test script scripts/run_tests.py

# Installation
install(DIRECTORY include/ DESTINATION include/someip FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION lib FILES_MATCHING PATTERN "libsomeip*")
