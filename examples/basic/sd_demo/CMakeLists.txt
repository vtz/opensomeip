set(_deployment_src "${CMAKE_CURRENT_SOURCE_DIR}/sd_demo_deployment.json")
set(_deployment_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
if(_deployment_dir STREQUAL "")
    set(_deployment_dir "${CMAKE_CURRENT_BINARY_DIR}")
endif()
set(_deployment_out "${_deployment_dir}/sd_demo_deployment.json")

# Both demos share one runtime directory. A POST_BUILD copy on each target
# races when they link in parallel, and it does not rerun when only this
# JSON changes. One generated output depends on the source file.
add_custom_command(
    OUTPUT "${_deployment_out}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_deployment_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_deployment_src}" "${_deployment_out}"
    DEPENDS "${_deployment_src}"
    COMMENT "Copy sd_demo deployment file"
    VERBATIM
)

add_executable(sd_demo_server server.cpp "${_deployment_out}")
target_link_libraries(sd_demo_server opensomeip)
target_include_directories(sd_demo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_source_files_properties("${_deployment_out}" PROPERTIES HEADER_FILE_ONLY TRUE)

add_executable(sd_demo_client client.cpp)
target_link_libraries(sd_demo_client opensomeip)
target_include_directories(sd_demo_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_dependencies(sd_demo_client sd_demo_server)
