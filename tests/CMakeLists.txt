# Test executables (GTest is fetched in main CMakeLists.txt)
if(BUILD_TESTS)
    add_executable(test_serialization test_serialization.cpp)
    target_link_libraries(test_serialization someip-serialization gtest_main)

    add_executable(test_message test_message.cpp)
    target_link_libraries(test_message someip-serialization gtest_main)

    # Session manager tests (core functionality is implemented)
    add_executable(test_session_manager test_session_manager.cpp)
    target_link_libraries(test_session_manager someip-core gtest_main)

    # Endpoint tests (placeholder until transport is implemented)
    add_executable(test_endpoint test_endpoint.cpp)
    target_link_libraries(test_endpoint someip-core gtest_main)

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

    # Register available tests
    add_test(NAME SerializationTest COMMAND test_serialization)
    add_test(NAME MessageTest COMMAND test_message)
    add_test(NAME SessionManagerTest COMMAND test_session_manager)
    add_test(NAME EndpointTest COMMAND test_endpoint)  # Placeholder test
    add_test(NAME RpcTest COMMAND test_rpc)
    add_test(NAME SdTest COMMAND test_sd)
    add_test(NAME EventsTest COMMAND test_events)
    add_test(NAME TcpTransportTest COMMAND test_tcp_transport)
    add_test(NAME UdpTransportTest COMMAND test_udp_transport)
    add_test(NAME TpTest COMMAND test_tp)
    add_test(NAME E2ETest COMMAND test_e2e)
endif()
