# Method Calls Example Build Configuration

# Server executable
add_executable(method_calls_server server.cpp)
target_link_libraries(method_calls_server opensomeip)
target_include_directories(method_calls_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Client executable
add_executable(method_calls_client client.cpp)
target_link_libraries(method_calls_client opensomeip)
target_include_directories(method_calls_client PRIVATE ${CMAKE_SOURCE_DIR}/include)


# Custom targets for convenience
add_custom_target(method_calls DEPENDS method_calls_server method_calls_client)
add_custom_target(run_method_calls_server
    COMMAND method_calls_server
    DEPENDS method_calls_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Method Calls Server"
)
add_custom_target(run_method_calls_client
    COMMAND method_calls_client
    DEPENDS method_calls_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Method Calls Client"
)
