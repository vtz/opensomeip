# Copyright (c) 2026 Vinicius Tadeu Zein
# SPDX-License-Identifier: Apache-2.0

"""Host library and the C++ tests built by the default CMake configuration.

POSIX sockets, dynamic allocation, and the C API. RTOS ports, static
allocation, and packaging stay in CMake.
"""

OPENSOMEIP_SRCS = [
    "src/common/result.cpp",
    "src/someip/types.cpp",
    "src/someip/message.cpp",
    "src/core/session_manager.cpp",
    "src/serialization/serializer.cpp",
    "src/transport/endpoint.cpp",
    "src/transport/udp_transport.cpp",
    "src/transport/tcp_transport.cpp",
    "src/transport/event_driven_udp_transport.cpp",
    "src/transport/event_driven_tcp_transport.cpp",
    "src/e2e/e2e_protection.cpp",
    "src/e2e/e2e_receive.cpp",
    "src/e2e/e2e_header.cpp",
    "src/e2e/e2e_crc.cpp",
    "src/e2e/e2e_profile_registry.cpp",
    "src/e2e/e2e_profiles/standard_profile.cpp",
    "src/rpc/rpc_client.cpp",
    "src/rpc/rpc_server.cpp",
    "src/sd/sd_message.cpp",
    "src/sd/sd_client.cpp",
    "src/sd/sd_server.cpp",
    "src/events/event_publisher.cpp",
    "src/events/event_subscriber.cpp",
    "src/tp/tp_segmenter.cpp",
    "src/tp/tp_reassembler.cpp",
    "src/tp/tp_manager.cpp",
]

OPENSOMEIP_INCLUDES = [
    "include",
    "include/platform/dynamic",
    "include/platform/posix",
]

_COPTS = [
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-fstack-protector-strong",
    "-pthread",
]

_LINKOPTS = ["-pthread"]

PAL_CORE_SRCS = [
    "src/someip/message.cpp",
    "src/someip/types.cpp",
    "src/common/result.cpp",
    "src/e2e/e2e_header.cpp",
    "src/e2e/e2e_crc.cpp",
]

def _host_library(name, local_defines = [], testonly = False, visibility = ["//visibility:public"]):
    native.cc_library(
        name = name,
        testonly = testonly,
        srcs = OPENSOMEIP_SRCS + [
            "src/common/callback_storage.h",
            "src/transport/transport_session.h",
        ],
        hdrs = native.glob(["include/**/*.h"]),
        includes = OPENSOMEIP_INCLUDES,
        copts = _COPTS,
        local_defines = local_defines,
        linkopts = _LINKOPTS,
        visibility = visibility,
    )

def opensomeip_targets():
    """Library targets for the default host configuration."""
    # External modules link this target. It does not define
    # SOMEIP_TCP_TEST_CONNECT_HOLD. CMake adds that definition only to the
    # test build of tcp_transport.cpp when BUILD_TESTS is on.
    _host_library("opensomeip")

    # The TCP handshake test calls set_test_outbound_connect_hold_ms. That
    # hook stays off //:opensomeip. The hold defaults to zero until the test
    # sets it.
    _host_library(
        "opensomeip_tcp_connect_hold",
        local_defines = ["SOMEIP_TCP_TEST_CONNECT_HOLD"],
        testonly = True,
        visibility = ["//visibility:private"],
    )

    native.cc_library(
        name = "opensomeip_capi",
        srcs = [
            "src/capi/opensomeip_message.cpp",
            "src/capi/opensomeip_serializer.cpp",
            "src/capi/opensomeip_transport.cpp",
            "src/capi/opensomeip_rpc.cpp",
            "src/capi/opensomeip_sd.cpp",
            "src/capi/opensomeip_events.cpp",
            "src/capi/opensomeip_tp.cpp",
            "src/capi/opensomeip_e2e.cpp",
            "src/capi/capi_internal.h",
        ],
        deps = [":opensomeip"],
        copts = _COPTS + ["-fexceptions"],
        linkopts = _LINKOPTS,
        visibility = ["//visibility:public"],
    )

def host_cc_test(name, src, extra_srcs = [], capi = False, hold_connect = False):
    """One C++ test that links the host library, or the C API when capi is set.

    hold_connect links the test-only library that defines
    SOMEIP_TCP_TEST_CONNECT_HOLD. The public //:opensomeip target does not.
    """
    if capi:
        library = ":opensomeip_capi"
    elif hold_connect:
        library = ":opensomeip_tcp_connect_hold"
    else:
        library = ":opensomeip"
    native.cc_test(
        name = name,
        srcs = [src, "tests/static_pool_init.h"] + extra_srcs,
        includes = ["tests"],
        deps = [
            library,
            "@googletest//:gtest_main",
        ],
        copts = _COPTS,
        linkopts = _LINKOPTS,
        tags = ["requires-network"],
    )

def pal_mock_cc_test(name, src, mock_dir, backend_dir):
    """PAL conformance test compiled against mock RTOS headers."""
    native.cc_test(
        name = name,
        srcs = [
            src,
            "tests/pal_conformance_tests.inc",
        ] + PAL_CORE_SRCS + native.glob([
            "include/**/*.h",
            mock_dir + "/**",
        ]),
        includes = [
            mock_dir,
            "include",
            "include/platform/dynamic",
            backend_dir,
        ],
        deps = ["@googletest//:gtest_main"],
        copts = _COPTS,
        linkopts = _LINKOPTS,
        tags = ["requires-network"],
    )

def freertos_sleep_cc_test(name, src, local_defines):
    """FreeRTOS sleep test compiled against the mock kernel headers."""
    native.cc_test(
        name = name,
        srcs = [src] + native.glob([
            "include/**/*.h",
            "tests/mocks/freertos/**",
        ]),
        includes = [
            "tests/mocks/freertos",
            "include",
            "include/platform/dynamic",
            "include/platform/freertos",
        ],
        local_defines = local_defines,
        deps = ["@googletest//:gtest_main"],
        copts = _COPTS,
        linkopts = _LINKOPTS,
        tags = ["requires-network"],
    )
