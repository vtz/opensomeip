set(_deployment_src "${CMAKE_CURRENT_SOURCE_DIR}/sd_demo_deployment.json")
set(_deployment_gen "${CMAKE_CURRENT_BINARY_DIR}/sd_demo_deployment.json")

# The generated file depends on the source, so a JSON-only edit reruns the
# copy. Each executable then copies into its own TARGET_FILE_DIR, which is
# the per-config directory on Visual Studio and Xcode. The client depends on
# the server, so those copies are not concurrent when both directories match.
add_custom_command(
    OUTPUT "${_deployment_gen}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_deployment_src}" "${_deployment_gen}"
    DEPENDS "${_deployment_src}"
    COMMENT "Stage sd_demo deployment file"
    VERBATIM
)
set_source_files_properties("${_deployment_gen}" PROPERTIES HEADER_FILE_ONLY TRUE)

add_executable(sd_demo_server server.cpp "${_deployment_gen}")
target_link_libraries(sd_demo_server opensomeip)
target_include_directories(sd_demo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_custom_command(
    TARGET sd_demo_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_deployment_gen}"
            "$<TARGET_FILE_DIR:sd_demo_server>/sd_demo_deployment.json"
    VERBATIM
)

add_executable(sd_demo_client client.cpp "${_deployment_gen}")
target_link_libraries(sd_demo_client opensomeip)
target_include_directories(sd_demo_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_dependencies(sd_demo_client sd_demo_server)
add_custom_command(
    TARGET sd_demo_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_deployment_gen}"
            "$<TARGET_FILE_DIR:sd_demo_client>/sd_demo_deployment.json"
    VERBATIM
)
