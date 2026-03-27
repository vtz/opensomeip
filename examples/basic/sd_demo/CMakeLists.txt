add_executable(sd_demo_server server.cpp)
target_link_libraries(sd_demo_server opensomeip)
target_include_directories(sd_demo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(sd_demo_client client.cpp)
target_link_libraries(sd_demo_client opensomeip)
target_include_directories(sd_demo_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
