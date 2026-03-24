################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
#
# Minimal BSP for STM32F407 on Renode: startup, linker script, UART retarget.
# Shared by FreeRTOS and ThreadX Renode test builds.
################################################################################

add_library(stm32f407_bsp STATIC
    startup.c
    uart_retarget.c
)

target_include_directories(stm32f407_bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Use the Renode-specific linker script
set(BSP_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/stm32f407_renode.ld"
    CACHE FILEPATH "Linker script for STM32F407 Renode BSP" FORCE)
