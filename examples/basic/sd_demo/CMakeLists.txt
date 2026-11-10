add_executable(sd_demo_server server.cpp)
target_link_libraries(sd_demo_server opensomeip)
target_include_directories(sd_demo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(sd_demo_client client.cpp)
target_link_libraries(sd_demo_client opensomeip)
target_include_directories(sd_demo_client PRIVATE ${CMAKE_SOURCE_DIR}/include)

foreach(tgt sd_demo_server sd_demo_client)
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/sd_demo_deployment.json
                $<TARGET_FILE_DIR:${tgt}>/sd_demo_deployment.json)
endforeach()
