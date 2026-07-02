# Test executables (GTest is fetched in main CMakeLists.txt)
if(BUILD_TESTS)
    add_executable(test_serialization test_serialization.cpp)
    target_link_libraries(test_serialization someip-serialization gtest_main)

    add_executable(test_message test_message.cpp)
    target_link_libraries(test_message someip-serialization gtest_main)

    # Session manager tests (core functionality is implemented)
    add_executable(test_session_manager test_session_manager.cpp)
    target_link_libraries(test_session_manager someip-core gtest_main)

    # Endpoint tests
    add_executable(test_endpoint test_endpoint.cpp)
    target_link_libraries(test_endpoint someip-transport gtest_main)

    # RPC tests
    add_executable(test_rpc test_rpc.cpp)
    target_link_libraries(test_rpc someip-rpc gtest_main)

# SD tests
add_executable(test_sd test_sd.cpp)
target_link_libraries(test_sd someip-sd gtest_main)

# Events tests
add_executable(test_events test_events.cpp)
target_link_libraries(test_events someip-events gtest_main)

# TCP Transport tests
add_executable(test_tcp_transport test_tcp_transport.cpp)
target_link_libraries(test_tcp_transport someip-transport gtest_main)

# UDP Transport tests
add_executable(test_udp_transport test_udp_transport.cpp)
target_link_libraries(test_udp_transport someip-transport gtest_main)

# TP tests
add_executable(test_tp test_tp.cpp)
target_link_libraries(test_tp someip-tp gtest_main)

# E2E tests
add_executable(test_e2e test_e2e.cpp)
target_link_libraries(test_e2e someip-core gtest_main)

    add_executable(test_platform_threading test_platform_threading.cpp)
    target_link_libraries(test_platform_threading someip-core gtest_main)

    # Winsock init for tests that use sockets (WSAStartup before main)
    if(WIN32)
        set(_SOCKET_TESTS
            test_platform_threading test_udp_transport test_tcp_transport
            test_rpc test_sd test_events)
        foreach(_t ${_SOCKET_TESTS})
            target_sources(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/winsock_init.cpp)
        endforeach()
    endif()

    # ---- PAL Conformance Tests (mock RTOS backends) ----
    #
    # Each test compiles the PAL backend (thread_impl.h, memory_impl.h) against
    # mock RTOS headers instead of the real RTOS SDK.  Core sources (message.cpp
    # etc.) are compiled directly so the mock include paths take full precedence.
    # Skipped on Windows: mock headers rely on POSIX arpa/inet.h.

    if(NOT WIN32)
        set(PAL_MOCK_CORE_SOURCES
            ${CMAKE_SOURCE_DIR}/src/someip/message.cpp
            ${CMAKE_SOURCE_DIR}/src/someip/types.cpp
            ${CMAKE_SOURCE_DIR}/src/common/result.cpp
            ${CMAKE_SOURCE_DIR}/src/e2e/e2e_header.cpp
            ${CMAKE_SOURCE_DIR}/src/e2e/e2e_crc.cpp
        )

        find_package(Threads REQUIRED)

        function(add_pal_mock_test TEST_NAME SOURCE_FILE MOCK_DIR BACKEND_DIR)
            add_executable(${TEST_NAME} ${SOURCE_FILE} ${PAL_MOCK_CORE_SOURCES})
            target_include_directories(${TEST_NAME} BEFORE PRIVATE
                ${MOCK_DIR}
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/include/platform/dynamic
                ${BACKEND_DIR}
            )
            target_link_libraries(${TEST_NAME} PRIVATE gtest_main Threads::Threads)
            target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
            add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        endfunction()

        # FreeRTOS PAL via mock (byteorder_impl.h/net_impl.h in mock dir)
        add_pal_mock_test(test_pal_freertos_mock
            test_pal_freertos_mock.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/mocks/freertos
            ${CMAKE_SOURCE_DIR}/include/platform/freertos
        )

        # ThreadX PAL via mock
        add_pal_mock_test(test_pal_threadx_mock
            test_pal_threadx_mock.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/mocks/threadx
            ${CMAKE_SOURCE_DIR}/include/platform/threadx
        )

        # Zephyr PAL via mock (mock dir shadows byteorder_impl.h/net_impl.h)
        add_pal_mock_test(test_pal_zephyr_mock
            test_pal_zephyr_mock.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/mocks/zephyr
            ${CMAKE_SOURCE_DIR}/include/platform/zephyr
        )
    endif()

    # ---- Static Allocation Tests ----
    if(SOMEIP_USE_STATIC_ALLOC)
        add_executable(test_buffer_pool test_buffer_pool.cpp)
        target_link_libraries(test_buffer_pool opensomeip gtest_main)
        add_test(NAME BufferPoolTest COMMAND test_buffer_pool)

        add_executable(test_static_message_pool test_static_message_pool.cpp)
        target_link_libraries(test_static_message_pool opensomeip gtest_main)
        add_test(NAME StaticMessagePoolTest COMMAND test_static_message_pool)

        add_executable(test_platform_containers test_platform_containers.cpp)
        target_link_libraries(test_platform_containers opensomeip gtest_main)
        add_test(NAME PlatformContainersTest COMMAND test_platform_containers)

        add_executable(test_static_alloc_integration test_static_alloc_integration.cpp
            $<TARGET_OBJECTS:someip_malloc_trap>)
        target_link_libraries(test_static_alloc_integration opensomeip gtest_main)
        add_test(NAME StaticAllocIntegrationTest COMMAND test_static_alloc_integration)

        set_tests_properties(
            BufferPoolTest StaticMessagePoolTest
            PlatformContainersTest StaticAllocIntegrationTest
            PROPERTIES TIMEOUT 30)

        # PAL conformance for the static-alloc backend.
        # Uses real static pools (memory.cpp, buffer_pool.cpp) + POSIX threading.
        if(NOT WIN32)
            add_executable(test_pal_static_alloc_mock
                test_pal_static_alloc_mock.cpp
                ${PAL_MOCK_CORE_SOURCES}
                ${CMAKE_SOURCE_DIR}/src/platform/static/memory.cpp
                ${CMAKE_SOURCE_DIR}/src/platform/static/buffer_pool.cpp
            )
            target_include_directories(test_pal_static_alloc_mock BEFORE PRIVATE
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/include/platform/static
                ${CMAKE_SOURCE_DIR}/include/platform/posix
            )
            target_link_libraries(test_pal_static_alloc_mock PRIVATE
                gtest_main Threads::Threads etl::etl)
            target_compile_features(test_pal_static_alloc_mock PRIVATE cxx_std_17)
            # DestructorWithoutJoinSafe expects non-terminating destruction,
            # but the POSIX Thread correctly calls std::terminate per
            # REQ_PAL_THREAD_DTOR_E01. Exclude it from this target.
            add_test(NAME test_pal_static_alloc_mock COMMAND test_pal_static_alloc_mock
                --gtest_filter=-ThreadConformance.DestructorWithoutJoinSafe)
            set_tests_properties(test_pal_static_alloc_mock PROPERTIES TIMEOUT 30)
        endif()
    endif()

    # Register available tests
    add_test(NAME PlatformThreadingTest COMMAND test_platform_threading)
    add_test(NAME SerializationTest COMMAND test_serialization)
    add_test(NAME MessageTest COMMAND test_message)
    add_test(NAME SessionManagerTest COMMAND test_session_manager)
    add_test(NAME EndpointTest COMMAND test_endpoint)
    add_test(NAME RpcTest COMMAND test_rpc)
    add_test(NAME SdTest COMMAND test_sd)
    add_test(NAME EventsTest COMMAND test_events)
    add_test(NAME TcpTransportTest COMMAND test_tcp_transport)
    add_test(NAME UdpTransportTest COMMAND test_udp_transport)
    add_test(NAME TpTest COMMAND test_tp)
    add_test(NAME E2ETest COMMAND test_e2e)

    # Per-test timeout (seconds) to catch hangs in local and CI runs
    set(_ALL_TESTS
        PlatformThreadingTest SerializationTest MessageTest
        SessionManagerTest EndpointTest RpcTest SdTest EventsTest
        TcpTransportTest UdpTransportTest TpTest E2ETest
    )
    if(NOT WIN32)
        list(APPEND _ALL_TESTS
            test_pal_freertos_mock test_pal_threadx_mock test_pal_zephyr_mock
        )
    endif()
    set_tests_properties(${_ALL_TESTS} PROPERTIES TIMEOUT 30)
endif()
