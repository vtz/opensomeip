# Examples organized in hierarchical structure
# Each subdirectory contains its own CMakeLists.txt

# Examples are demonstration code; don't fail the build on warnings.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-error=unused-parameter -Wno-error=unused-const-variable)
endif()

# Basic Examples
add_subdirectory(basic/hello_world)
add_subdirectory(basic/method_calls)
add_subdirectory(basic/sd_demo)
add_subdirectory(basic/events)

# Advanced Examples
add_subdirectory(advanced/complex_types)
add_subdirectory(advanced/large_messages)
add_subdirectory(advanced/multi_service)
add_subdirectory(advanced/udp_config)

# E2E Protection Examples
add_subdirectory(e2e_protection)

# Interoperability Testing (requires vsomeip3 and Boost - off by default)
if(BUILD_VSOMEIP_INTEROP)
    add_subdirectory(vsomeip_interop)
endif()

# Renode-specific examples (only built when Renode test presets are active)
if(SOMEIP_FREERTOS_RENODE_TESTS)
    add_subdirectory(renode/freertos_multitask)
endif()
if(SOMEIP_THREADX_RENODE_TESTS)
    add_subdirectory(renode/threadx_pool_demo)
endif()
