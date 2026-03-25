# Multi-Service Example Build Configuration

# Server executable
add_executable(multi_service_server server.cpp)
target_link_libraries(multi_service_server opensomeip)
target_include_directories(multi_service_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Client executable
add_executable(multi_service_client client.cpp)
target_link_libraries(multi_service_client opensomeip)
target_include_directories(multi_service_client PRIVATE ${CMAKE_SOURCE_DIR}/include)


# Custom targets for convenience
add_custom_target(multi_service DEPENDS multi_service_server multi_service_client)
add_custom_target(run_multi_service_server
    COMMAND multi_service_server
    DEPENDS multi_service_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Multi-Service Server"
)
add_custom_target(run_multi_service_client
    COMMAND multi_service_client
    DEPENDS multi_service_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Multi-Service Client"
)
