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
# Build System Configuration
# @implements REQ_ARCH_005, REQ_ARCH_006, REQ_ARCH_007
# @brief CMake build configuration for SOME/IP stack with cross-platform support,
#        code quality tools integration, and safety-critical build options
################################################################################

cmake_minimum_required(VERSION 3.20)

# Read version from VERSION file
# Use CMAKE_CURRENT_SOURCE_DIR to support being used as a subdirectory
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION)
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
option(ENABLE_WERROR "Treat compiler warnings as errors (recommended for CI)" OFF)

# Platform backend selection
option(SOMEIP_USE_FREERTOS "Use FreeRTOS for threading primitives" OFF)
option(SOMEIP_USE_THREADX  "Use ThreadX for threading primitives" OFF)
option(SOMEIP_USE_LWIP     "Use lwIP for network sockets" OFF)

# RTOS linux/POSIX port runtime tests (Linux only)
option(SOMEIP_FREERTOS_LINUX_TESTS "Build and run FreeRTOS runtime tests using FreeRTOS POSIX port" OFF)
option(SOMEIP_THREADX_LINUX_TESTS "Build and run ThreadX runtime tests using Eclipse ThreadX linux port" OFF)

# Derive backend from runtime test mode before include-path/backend selection.
if(SOMEIP_FREERTOS_LINUX_TESTS AND SOMEIP_THREADX_LINUX_TESTS)
    message(FATAL_ERROR "SOMEIP_FREERTOS_LINUX_TESTS and SOMEIP_THREADX_LINUX_TESTS are mutually exclusive")
endif()
if(SOMEIP_FREERTOS_LINUX_TESTS AND NOT WIN32)
    set(SOMEIP_USE_FREERTOS ON CACHE BOOL "" FORCE)
elseif(SOMEIP_THREADX_LINUX_TESTS AND NOT WIN32)
    set(SOMEIP_USE_THREADX ON CACHE BOOL "" FORCE)
endif()

# Fetch RTOS kernels and lwIP early — before add_subdirectory(src) — so their
# headers are on the include path when PAL *_impl.h files are compiled.

# --- FreeRTOS ---
if(SOMEIP_USE_FREERTOS)
    enable_language(C)

    if(SOMEIP_FREERTOS_LINUX_TESTS AND NOT WIN32)
        set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "" FORCE)
    elseif(NOT DEFINED CACHE{FREERTOS_PORT})
        set(FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "FreeRTOS port (e.g. GCC_ARM_CM4F, GCC_ARM_CM7)")
    endif()
    set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE
        ${PROJECT_SOURCE_DIR}/tests/freertos
    )

    FetchContent_Declare(freertos_kernel
        GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
        GIT_TAG        V11.1.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(freertos_kernel)

    include_directories(
        ${freertos_kernel_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/tests/freertos
    )
    if(SOMEIP_FREERTOS_LINUX_TESTS AND NOT WIN32)
        include_directories(${freertos_kernel_SOURCE_DIR}/portable/ThirdParty/GCC/Posix)
    endif()
endif()

# --- ThreadX ---
if(SOMEIP_USE_THREADX)
    if(SOMEIP_THREADX_LINUX_TESTS AND NOT WIN32)
        set(THREADX_ARCH "linux" CACHE STRING "" FORCE)
    elseif(NOT DEFINED CACHE{THREADX_ARCH})
        set(THREADX_ARCH "cortex_m4" CACHE STRING "ThreadX architecture (e.g. cortex_m4, cortex_m7)")
    endif()
    if(NOT DEFINED CACHE{THREADX_TOOLCHAIN})
        set(THREADX_TOOLCHAIN "gnu" CACHE STRING "ThreadX toolchain")
    endif()

    FetchContent_Declare(threadx
        GIT_REPOSITORY https://github.com/eclipse-threadx/threadx.git
        GIT_TAG        v6.4.1_rel
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(threadx)

    include_directories(
        ${threadx_SOURCE_DIR}/common/inc
        ${threadx_SOURCE_DIR}/ports/${THREADX_ARCH}/${THREADX_TOOLCHAIN}/inc
    )
endif()

# --- lwIP (headers only — we don't build lwIP itself, the user's BSP does) ---
# When SOMEIP_USE_LWIP is ON, fetch lwIP headers so net_impl.h and
# byteorder_impl.h can compile.  Users who already have lwIP in their project
# can point FETCHCONTENT_SOURCE_DIR_LWIP to their own copy.
if(SOMEIP_USE_LWIP)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    FetchContent_Declare(lwip
        GIT_REPOSITORY https://github.com/lwip-tcpip/lwip.git
        GIT_TAG        STABLE-2_2_0_RELEASE
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(lwip)
    if(NOT lwip_POPULATED)
        FetchContent_Populate(lwip)
    endif()

    include_directories(
        ${lwip_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/cmake/config/lwip
    )
endif()

# Set policy for FetchContent timestamp handling (CMake 3.24+)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Fetch Google Test
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # Prevent overriding the parent project's compiler/linker on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    # Set GTest variables for compatibility
    set(GTEST_LIBRARIES gtest gtest_main)
    set(GTEST_INCLUDE_DIRS ${googletest_SOURCE_DIR}/include)
endif()

# Compiler warnings: always enabled for early detection of issues
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    if(ENABLE_WERROR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()
endif()

# Additional compiler flags for safety-critical builds
if(SAFETY_LEVEL STREQUAL "ASIL_B" OR SAFETY_LEVEL STREQUAL "ASIL_C" OR SAFETY_LEVEL STREQUAL "ASIL_D")
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

# Platform backend include directories (selects which *_impl.h files are found)
if(SOMEIP_USE_FREERTOS)
    include_directories(include/platform/freertos)
elseif(SOMEIP_USE_THREADX)
    include_directories(include/platform/threadx)
elseif(WIN32)
    include_directories(include/platform/win32)
else()
    include_directories(include/platform/posix)
endif()

if(SOMEIP_USE_LWIP)
    include_directories(include/platform/lwip)
    add_compile_definitions(SOMEIP_NET_LWIP)
elseif(WIN32)
    # win32 net/byteorder already included above
else()
    # posix net/byteorder already included above
endif()

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
#         --config-file=${PROJECT_SOURCE_DIR}/.clang-tidy
#         --header-filter=.*)
# endif()

# Add custom targets for code quality
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND find ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/examples ${PROJECT_SOURCE_DIR}/tests
               -name "*.cpp" -o -name "*.h" |
               xargs ${CLANG_FORMAT_EXE} -i
        COMMENT "Formatting code with clang-format"
    )
endif()

if(CLANG_TIDY_EXE)
    # Check if test targets exist (indicates successful googletest setup)
    if(TARGET test_message)
        add_custom_target(tidy
            COMMAND ${PROJECT_SOURCE_DIR}/scripts/run_clang_tidy.sh
                    ${CLANG_TIDY_EXE}
                    ${PROJECT_SOURCE_DIR}/.clang-tidy
                    ${CMAKE_BINARY_DIR}
                    ${PROJECT_SOURCE_DIR}
            COMMENT "Running clang-tidy static analysis on all source files"
        )
    else()
        add_custom_target(tidy
            COMMAND ${PROJECT_SOURCE_DIR}/scripts/run_clang_tidy.sh
                    ${CLANG_TIDY_EXE}
                    ${PROJECT_SOURCE_DIR}/.clang-tidy
                    ${CMAKE_BINARY_DIR}
                    ${PROJECT_SOURCE_DIR}
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

# RTOS runtime tests (linux/POSIX port only).
if(SOMEIP_FREERTOS_LINUX_TESTS AND NOT WIN32)
    enable_testing()
    add_subdirectory(tests/freertos)
endif()
if(SOMEIP_THREADX_LINUX_TESTS AND NOT WIN32)
    enable_testing()
    add_subdirectory(tests/threadx)
endif()

if(BUILD_TESTS AND NOT SOMEIP_FREERTOS_LINUX_TESTS AND NOT SOMEIP_THREADX_LINUX_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Note: Tools are scripts/utilities and don't require CMake building
# BUILD_TOOLS option is kept for future CMake-based tools

# Note: Coverage reporting is handled by the test script scripts/run_tests.py

################################################################################
# Requirements Documentation and Traceability
################################################################################

# Find Python3 for requirements scripts
find_package(Python3 COMPONENTS Interpreter)

# Find Sphinx for documentation
find_program(SPHINX_BUILD sphinx-build
    PATHS /usr/local/bin /usr/bin ~/.local/bin
)

# Requirements documentation directory
set(REQUIREMENTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/requirements)
set(REQUIREMENTS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/requirements)
set(TRACEABILITY_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/traceability)

# Build open-someip-spec to generate needs.json (if available)
set(SPEC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/open-someip-spec/src)
set(SPEC_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/open-someip-spec/build)
set(SPEC_NEEDS_JSON ${SPEC_BUILD_DIR}/needs.json)

if(Python3_FOUND)
    # --- Targets that only need Python3 (no Sphinx) ---

    # Extract @implements, @tests, @satisfies annotations from source code
    add_custom_target(extract_code_requirements
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_code_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --output ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --src-dirs src include
                --test-dirs tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Extracting requirement traceability from source code"
    )

    # Validate requirements completeness and traceability
    add_custom_target(requirements_check
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS extract_code_requirements
        COMMENT "Validating requirements completeness"
    )

    # Strict validation (fails on any gap)
    add_custom_target(requirements_check_strict
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --strict
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS extract_code_requirements
        COMMENT "Validating requirements completeness (strict mode)"
    )

    # Generate traceability matrix documents
    add_custom_target(traceability_matrix
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TRACEABILITY_BUILD_DIR}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_traceability_matrix.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --output-dir ${TRACEABILITY_BUILD_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS extract_code_requirements
        COMMENT "Generating traceability matrix"
    )

    # Check spec requirement mappings
    add_custom_target(spec_check
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_spec_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --output ${TRACEABILITY_BUILD_DIR}/spec_mapping_report.md
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking spec requirement mappings"
    )

    # Verify implementation status against RST definitions
    add_custom_target(implementation_status
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_implementation_status.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --output ${TRACEABILITY_BUILD_DIR}/implementation_verification.md
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS extract_code_requirements
        COMMENT "Verifying implementation status"
    )

    # Generate implementation report
    add_custom_target(implementation_report
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_implementation_report.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --output ${TRACEABILITY_BUILD_DIR}/implementation_report.md
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating implementation report"
    )

    # --- Combined target: run everything and produce all traceability docs ---
    add_custom_target(traceability_docs
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TRACEABILITY_BUILD_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo "=== Extracting code references ==="
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_code_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --output ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --src-dirs src include
                --test-dirs tests
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== Validating requirements ==="
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== Generating traceability matrix ==="
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_traceability_matrix.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --output-dir ${TRACEABILITY_BUILD_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== Checking spec mappings ==="
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_spec_requirements.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --output ${TRACEABILITY_BUILD_DIR}/spec_mapping_report.md
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== Verifying implementation status ==="
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_implementation_status.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --code-refs ${CMAKE_CURRENT_BINARY_DIR}/code_references.json
                --output ${TRACEABILITY_BUILD_DIR}/implementation_verification.md
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== Generating implementation report ==="
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_implementation_report.py
                --project-root ${CMAKE_CURRENT_SOURCE_DIR}
                --output ${TRACEABILITY_BUILD_DIR}/implementation_report.md
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== All traceability docs written to ${TRACEABILITY_BUILD_DIR} ==="
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating all traceability documentation"
    )

    # --- Targets that additionally need Sphinx ---
    if(SPHINX_BUILD)
        add_custom_target(spec_docs
            COMMAND ${SPHINX_BUILD} -b needs ${SPEC_SOURCE_DIR} ${SPEC_BUILD_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/open-someip-spec
            COMMENT "Building open-someip-spec documentation to generate needs.json"
        )

        add_custom_target(requirements_docs
            COMMAND ${CMAKE_COMMAND} -E make_directory ${REQUIREMENTS_BUILD_DIR}
            COMMAND ${SPHINX_BUILD} -b html ${REQUIREMENTS_SOURCE_DIR} ${REQUIREMENTS_BUILD_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS extract_code_requirements
            COMMENT "Building requirements documentation (Sphinx HTML)"
        )

        message(STATUS "Requirements targets available: traceability_docs, requirements_docs, traceability_matrix, requirements_check, requirements_check_strict, spec_check, implementation_status, implementation_report")
    else()
        add_custom_target(requirements_docs
            COMMAND ${CMAKE_COMMAND} -E echo "Sphinx not found. Install with: pip install sphinx sphinx-needs"
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Sphinx not available - install sphinx and sphinx-needs"
        )

        message(STATUS "Requirements targets available: traceability_docs, traceability_matrix, requirements_check, requirements_check_strict, spec_check, implementation_status, implementation_report (Sphinx not found - requirements_docs unavailable)")
    endif()
else()
    # No Python3 at all — create placeholder targets so the build graph is complete
    message(STATUS "Python3 not found - requirements documentation targets will produce diagnostics only")

    add_custom_target(traceability_docs
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found. Requirements tools are unavailable."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Python3 not available"
    )
    add_custom_target(requirements_docs
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(traceability_matrix
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(requirements_check
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(extract_code_requirements
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found. Cannot extract code requirements."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(requirements_check_strict
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found. Cannot run strict requirements check."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(spec_check
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found. Cannot check spec requirements."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(implementation_status
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found. Cannot verify implementation status."
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(implementation_report
        COMMAND ${CMAKE_COMMAND} -E echo "Python3 not found. Cannot generate implementation report."
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

################################################################################
# Installation
################################################################################

# Installation
install(DIRECTORY include/ DESTINATION include/someip FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION lib FILES_MATCHING PATTERN "libsomeip*")
