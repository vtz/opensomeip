# E2E Protection Examples

# Basic E2E protection example
add_executable(basic_e2e basic_e2e.cpp)
target_link_libraries(basic_e2e opensomeip)

# Plugin integration example
add_executable(plugin_integration plugin_integration.cpp)
target_link_libraries(plugin_integration opensomeip)

# Safety-critical example
add_executable(safety_critical safety_critical.cpp)
target_link_libraries(safety_critical opensomeip)
