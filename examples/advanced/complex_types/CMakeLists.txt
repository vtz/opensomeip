# Complex Types Example Build Configuration

# Server executable
add_executable(complex_types_server server.cpp)
target_link_libraries(complex_types_server opensomeip)
target_include_directories(complex_types_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Client executable
add_executable(complex_types_client client.cpp)
target_link_libraries(complex_types_client opensomeip)
target_include_directories(complex_types_client PRIVATE ${CMAKE_SOURCE_DIR}/include)


# Custom targets for convenience
add_custom_target(complex_types DEPENDS complex_types_server complex_types_client)
add_custom_target(run_complex_types_server
    COMMAND complex_types_server
    DEPENDS complex_types_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Complex Types Server"
)
add_custom_target(run_complex_types_client
    COMMAND complex_types_client
    DEPENDS complex_types_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Complex Types Client"
)
