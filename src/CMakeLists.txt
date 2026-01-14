# Core library sources
set(CORE_SOURCES
    common/result.cpp
    someip/types.cpp
    someip/message.cpp
    core/session_manager.cpp
)

# Serialization library sources
set(SERIALIZATION_SOURCES
    serialization/serializer.cpp
)

# Transport library sources
set(TRANSPORT_SOURCES
    transport/endpoint.cpp
    transport/udp_transport.cpp
    transport/tcp_transport.cpp
)

# E2E library sources (defined before core since core depends on E2E header)
set(E2E_SOURCES
    e2e/e2e_protection.cpp
    e2e/e2e_header.cpp
    e2e/e2e_crc.cpp
    e2e/e2e_profile_registry.cpp
    e2e/e2e_profiles/standard_profile.cpp
)

# Use OBJECT libraries to avoid circular dependency between core and E2E
# Both are compiled separately but linked together

# Core object library
add_library(someip-core-obj OBJECT ${CORE_SOURCES})
target_include_directories(someip-core-obj PUBLIC ${CMAKE_SOURCE_DIR}/include)

# E2E object library
add_library(someip-e2e-obj OBJECT ${E2E_SOURCES})
target_include_directories(someip-e2e-obj PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Create combined core library that includes both core and E2E objects
# This avoids circular dependency issues at link time
add_library(someip-core STATIC
    $<TARGET_OBJECTS:someip-core-obj>
    $<TARGET_OBJECTS:someip-e2e-obj>
)
target_include_directories(someip-core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-core PUBLIC Threads::Threads)

# Create E2E library as an alias to core (for backward compatibility)
# All E2E functionality is now part of someip-core
add_library(someip-e2e ALIAS someip-core)

# Create serialization library (depends on core)
add_library(someip-serialization STATIC ${SERIALIZATION_SOURCES})
target_include_directories(someip-serialization PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-serialization PUBLIC someip-core)

# RPC library sources
set(RPC_SOURCES
    rpc/rpc_client.cpp
    rpc/rpc_server.cpp
)

# SD library sources
set(SD_SOURCES
    sd/sd_message.cpp
    sd/sd_client.cpp
    sd/sd_server.cpp
)

# Events library sources
set(EVENTS_SOURCES
    events/event_publisher.cpp
    events/event_subscriber.cpp
)

# TP library sources
set(TP_SOURCES
    tp/tp_segmenter.cpp
    tp/tp_reassembler.cpp
    tp/tp_manager.cpp
)

# Create transport library (depends on core + threading)
add_library(someip-transport STATIC ${TRANSPORT_SOURCES})
target_include_directories(someip-transport PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-transport PRIVATE someip-core PRIVATE Threads::Threads)

# Create RPC library (depends on core, transport, serialization)
add_library(someip-rpc STATIC ${RPC_SOURCES})
target_include_directories(someip-rpc PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-rpc PRIVATE someip-core PRIVATE someip-transport PRIVATE someip-serialization)

# Create SD library (depends on core, transport)
add_library(someip-sd STATIC ${SD_SOURCES})
target_include_directories(someip-sd PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-sd PRIVATE someip-core PRIVATE someip-transport)

# Create Events library (depends on core, transport, rpc)
add_library(someip-events STATIC ${EVENTS_SOURCES})
target_include_directories(someip-events PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-events PRIVATE someip-core PRIVATE someip-transport PRIVATE someip-rpc)

# Create TP library (depends on core, transport)
add_library(someip-tp STATIC ${TP_SOURCES})
target_include_directories(someip-tp PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(someip-tp PRIVATE someip-core PRIVATE someip-transport)

# Create convenience aliases for backward compatibility
add_library(someip-common ALIAS someip-core)

# Find threading library properly
find_package(Threads REQUIRED)

# Set library properties (only on real targets, not aliases)
set_target_properties(someip-core someip-serialization someip-transport PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
