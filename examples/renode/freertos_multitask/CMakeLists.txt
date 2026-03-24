################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
#
# FreeRTOS multi-task SOME/IP demo for Renode (STM32F407 Cortex-M4F).
# Build with: cmake --preset freertos-cortexm4-renode
################################################################################

# This CMakeLists.txt is not added automatically by the root build.
# It documents how to build the example standalone or integrate it.
# When SOMEIP_FREERTOS_RENODE_TESTS is ON, the BSP and FreeRTOS kernel
# are already available in the build tree.

if(SOMEIP_FREERTOS_RENODE_TESTS)
    add_executable(freertos_multitask_demo main.cpp)

    target_include_directories(freertos_multitask_demo PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/bsp/stm32f407_renode
    )

    target_link_libraries(freertos_multitask_demo PRIVATE
        someip-core
        someip-serialization
        stm32f407_bsp
    )

    target_link_options(freertos_multitask_demo PRIVATE
        -T${BSP_LINKER_SCRIPT}
        -Wl,--gc-sections
        -Wl,--print-memory-usage
    )

    set_target_properties(freertos_multitask_demo PROPERTIES
        SUFFIX ".elf"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/renode"
    )
endif()
