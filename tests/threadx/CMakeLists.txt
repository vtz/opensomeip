# ThreadX linux port runtime tests
# Only built when SOMEIP_THREADX_LINUX_TESTS=ON (Linux + GCC)

add_executable(test_threadx_core test_threadx_core.cpp)

target_include_directories(test_threadx_core PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SOMEIP_THREADING_IMPL_DIR}
    ${SOMEIP_NET_IMPL_DIR}
)

target_link_libraries(test_threadx_core PRIVATE
    someip-core
    someip-serialization
    someip-transport
)

add_test(NAME ThreadXCoreTest COMMAND test_threadx_core)
set_tests_properties(ThreadXCoreTest PROPERTIES TIMEOUT 30)
