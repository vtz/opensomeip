# Large Messages Example Build Configuration

# Server executable
add_executable(large_messages_server server.cpp)
target_link_libraries(large_messages_server opensomeip)
target_include_directories(large_messages_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Client executable
add_executable(large_messages_client client.cpp)
target_link_libraries(large_messages_client opensomeip)
target_include_directories(large_messages_client PRIVATE ${CMAKE_SOURCE_DIR}/include)


# Custom targets for convenience
add_custom_target(large_messages DEPENDS large_messages_server large_messages_client)
add_custom_target(run_large_messages_server
    COMMAND large_messages_server
    DEPENDS large_messages_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Large Messages Server"
)
add_custom_target(run_large_messages_client
    COMMAND large_messages_client
    DEPENDS large_messages_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Large Messages Client"
)
