# FreeRTOS POSIX port runtime tests
# Only built when SOMEIP_FREERTOS_LINUX_TESTS=ON (Linux + GCC)

add_executable(test_freertos_core test_freertos_core.cpp)

target_include_directories(test_freertos_core PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SOMEIP_THREADING_IMPL_DIR}
    ${SOMEIP_NET_IMPL_DIR}
)

target_link_libraries(test_freertos_core PRIVATE
    someip-core
    someip-serialization
    someip-transport
)

add_test(NAME FreeRTOSCoreTest COMMAND test_freertos_core)
set_tests_properties(FreeRTOSCoreTest PROPERTIES TIMEOUT 30)
