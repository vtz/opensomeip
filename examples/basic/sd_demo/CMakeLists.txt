add_executable(sd_demo_server server.cpp)
target_link_libraries(sd_demo_server someip-sd someip-transport)
target_include_directories(sd_demo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
