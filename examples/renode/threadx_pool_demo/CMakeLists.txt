################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
#
# ThreadX memory pool SOME/IP demo for Renode (STM32F407 Cortex-M4).
# Build with: cmake --preset threadx-cortexm4-renode
################################################################################

if(SOMEIP_THREADX_RENODE_TESTS)
    add_executable(threadx_pool_demo main.cpp)

    target_include_directories(threadx_pool_demo PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/bsp/stm32f407_renode
    )

    target_sources(threadx_pool_demo PRIVATE
        ${PROJECT_SOURCE_DIR}/bsp/stm32f407_renode/tx_initialize_low_level.c
    )
    target_compile_definitions(threadx_pool_demo PRIVATE TX_INCLUDE_USER_DEFINE_FILE)
    target_link_libraries(threadx_pool_demo PRIVATE
        someip-core
        someip-serialization
        stm32f407_bsp
    )

    target_link_options(threadx_pool_demo PRIVATE
        -T${BSP_LINKER_SCRIPT}
        -Wl,--gc-sections
        -Wl,--print-memory-usage
    )

    set_target_properties(threadx_pool_demo PROPERTIES
        SUFFIX ".elf"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/renode"
    )
endif()
