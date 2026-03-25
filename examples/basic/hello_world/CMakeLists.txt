# Hello World Example Build Configuration

# Server executable
add_executable(hello_world_server server.cpp)
target_link_libraries(hello_world_server opensomeip)
target_include_directories(hello_world_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Client executable
add_executable(hello_world_client client.cpp)
target_link_libraries(hello_world_client opensomeip)
target_include_directories(hello_world_client PRIVATE ${CMAKE_SOURCE_DIR}/include)


# Custom targets for convenience
add_custom_target(hello_world DEPENDS hello_world_server hello_world_client)
add_custom_target(run_hello_world_server
    COMMAND hello_world_server
    DEPENDS hello_world_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Hello World Server"
)
add_custom_target(run_hello_world_client
    COMMAND hello_world_client
    DEPENDS hello_world_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Hello World Client"
)
