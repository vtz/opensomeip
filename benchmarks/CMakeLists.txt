################################################################################
# Copyright (c) 2026 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

add_executable(host_bench host_bench.cpp)
target_link_libraries(host_bench PRIVATE opensomeip)

# Pass git SHA at compile time
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE _git_sha
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(_git_sha)
    target_compile_definitions(host_bench PRIVATE "HOST_BENCH_GIT_SHA=\"${_git_sha}\"")
endif()

# Pass static footprint at compile time
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/scripts/static_memory_budget.py")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/static_memory_budget.py" --json
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _budget_json
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _budget_rc
    )
    if(_budget_rc EQUAL 0)
        string(REGEX MATCH "\"total_bytes\": *([0-9]+)" _match "${_budget_json}")
        if(CMAKE_MATCH_1)
            target_compile_definitions(host_bench PRIVATE "HOST_BENCH_STATIC_FOOTPRINT=${CMAKE_MATCH_1}")
        endif()
    endif()
endif()

# Ensure the output directory exists at build time
add_custom_command(TARGET host_bench POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/perf"
)
