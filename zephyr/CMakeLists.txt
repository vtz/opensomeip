################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

# Zephyr module CMakeLists.txt for the SOME/IP stack.
# Compiles the same source files as the host build but under Zephyr's build system.

if(CONFIG_SOMEIP)

set(SOMEIP_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(SOMEIP_INC  ${SOMEIP_ROOT}/include)
set(SOMEIP_SRC  ${SOMEIP_ROOT}/src)

zephyr_include_directories(${SOMEIP_INC})
if(CONFIG_ARCH_POSIX)
    zephyr_include_directories(${SOMEIP_INC}/platform/posix)
else()
    zephyr_include_directories(${SOMEIP_INC}/platform/zephyr)
endif()

# ---- Core (always built) ----
zephyr_library_named(someip_core)
zephyr_library_sources(
    ${SOMEIP_SRC}/common/result.cpp
    ${SOMEIP_SRC}/someip/types.cpp
    ${SOMEIP_SRC}/someip/message.cpp
    ${SOMEIP_SRC}/core/session_manager.cpp
    ${SOMEIP_SRC}/serialization/serializer.cpp
    ${SOMEIP_SRC}/transport/endpoint.cpp
    ${SOMEIP_SRC}/e2e/e2e_header.cpp
)
zephyr_library_compile_options(-std=c++17)

# ---- E2E ----
if(CONFIG_SOMEIP_E2E)
    zephyr_library_sources(
        ${SOMEIP_SRC}/e2e/e2e_protection.cpp
        ${SOMEIP_SRC}/e2e/e2e_crc.cpp
        ${SOMEIP_SRC}/e2e/e2e_profile_registry.cpp
        ${SOMEIP_SRC}/e2e/e2e_profiles/standard_profile.cpp
    )
endif()

# ---- TP ----
if(CONFIG_SOMEIP_TP)
    zephyr_library_sources(
        ${SOMEIP_SRC}/tp/tp_segmenter.cpp
        ${SOMEIP_SRC}/tp/tp_reassembler.cpp
        ${SOMEIP_SRC}/tp/tp_manager.cpp
    )
endif()

# ---- Transport ----
if(CONFIG_SOMEIP_TRANSPORT_UDP)
    zephyr_library_sources(
        ${SOMEIP_SRC}/transport/udp_transport.cpp
    )
endif()

if(CONFIG_SOMEIP_TRANSPORT_TCP)
    zephyr_library_sources(
        ${SOMEIP_SRC}/transport/tcp_transport.cpp
    )
endif()

# ---- SD ----
if(CONFIG_SOMEIP_SD)
    zephyr_library_sources(
        ${SOMEIP_SRC}/sd/sd_message.cpp
        ${SOMEIP_SRC}/sd/sd_client.cpp
        ${SOMEIP_SRC}/sd/sd_server.cpp
    )
endif()

# ---- RPC ----
if(CONFIG_SOMEIP_RPC)
    zephyr_library_sources(
        ${SOMEIP_SRC}/rpc/rpc_client.cpp
        ${SOMEIP_SRC}/rpc/rpc_server.cpp
    )
endif()

# ---- Events ----
if(CONFIG_SOMEIP_EVENTS)
    zephyr_library_sources(
        ${SOMEIP_SRC}/events/event_publisher.cpp
        ${SOMEIP_SRC}/events/event_subscriber.cpp
    )
endif()

# ---- Platform abstractions (Zephyr-specific implementations) ----
if(NOT CONFIG_ARCH_POSIX)
    zephyr_library_sources_ifdef(CONFIG_SOMEIP
        ${SOMEIP_SRC}/platform/zephyr/thread.cpp
        ${SOMEIP_SRC}/platform/zephyr/memory.cpp
    )
endif()

endif() # CONFIG_SOMEIP
