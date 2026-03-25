# Events Example Build Configuration

# Publisher executable
add_executable(events_publisher publisher.cpp)
target_link_libraries(events_publisher opensomeip)
target_include_directories(events_publisher PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Subscriber executable
add_executable(events_subscriber subscriber.cpp)
target_link_libraries(events_subscriber opensomeip)
target_include_directories(events_subscriber PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Custom targets for convenience
add_custom_target(events DEPENDS events_publisher events_subscriber)
add_custom_target(run_events_publisher
    COMMAND events_publisher
    DEPENDS events_publisher
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Events Publisher"
)
add_custom_target(run_events_subscriber
    COMMAND events_subscriber
    DEPENDS events_subscriber
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Events Subscriber"
)
