# All library sources consolidated into a single target.
# Linker --gc-sections handles dead code elimination for consumers that only
# use a subset of the stack (~570K total, 30 object files).
set(OPENSOMEIP_SOURCES
    common/result.cpp
    someip/types.cpp
    someip/message.cpp
    core/session_manager.cpp
    serialization/serializer.cpp
    transport/endpoint.cpp
    transport/udp_transport.cpp
    transport/tcp_transport.cpp
    e2e/e2e_protection.cpp
    e2e/e2e_header.cpp
    e2e/e2e_crc.cpp
    e2e/e2e_profile_registry.cpp
    e2e/e2e_profiles/standard_profile.cpp
    rpc/rpc_client.cpp
    rpc/rpc_server.cpp
    sd/sd_message.cpp
    sd/sd_client.cpp
    sd/sd_server.cpp
    events/event_publisher.cpp
    events/event_subscriber.cpp
    tp/tp_segmenter.cpp
    tp/tp_reassembler.cpp
    tp/tp_manager.cpp
)

# --- Platform backend selection via include path ---
# Threading / memory backend
if(SOMEIP_USE_FREERTOS)
    set(SOMEIP_THREADING_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/freertos)
elseif(SOMEIP_USE_THREADX)
    set(SOMEIP_THREADING_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/threadx)
elseif(WIN32)
    set(SOMEIP_THREADING_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/win32)
else()
    set(SOMEIP_THREADING_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/posix)
endif()

# Networking backend
if(SOMEIP_USE_LWIP)
    set(SOMEIP_NET_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/lwip)
elseif(WIN32)
    set(SOMEIP_NET_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/win32)
else()
    set(SOMEIP_NET_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/posix)
endif()

# Allocation backend (listed before threading so static memory_impl.h shadows RTOS headers)
if(SOMEIP_USE_STATIC_ALLOC)
    set(SOMEIP_ALLOC_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/static)
else()
    set(SOMEIP_ALLOC_IMPL_DIR ${PROJECT_SOURCE_DIR}/include/platform/dynamic)
endif()

# Library type follows BUILD_SHARED_LIBS (default: STATIC)
add_library(opensomeip ${OPENSOMEIP_SOURCES})
target_include_directories(opensomeip PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${SOMEIP_ALLOC_IMPL_DIR}
    ${SOMEIP_THREADING_IMPL_DIR}
    ${SOMEIP_NET_IMPL_DIR}
)

# Threading: only link pthreads on non-RTOS builds
if(NOT SOMEIP_USE_FREERTOS AND NOT SOMEIP_USE_THREADX)
    find_package(Threads REQUIRED)
    target_link_libraries(opensomeip PUBLIC Threads::Threads)
endif()

# RTOS kernel linkage
if(SOMEIP_USE_FREERTOS AND TARGET freertos_kernel)
    target_link_libraries(opensomeip PUBLIC freertos_kernel)
endif()
if(SOMEIP_USE_THREADX AND TARGET threadx)
    target_link_libraries(opensomeip PUBLIC threadx)
endif()

if(WIN32)
    target_link_libraries(opensomeip PUBLIC ws2_32)
endif()

# FreeRTOS platform sources (skipped when static allocation owns memory backend)
if(SOMEIP_USE_FREERTOS AND NOT SOMEIP_USE_STATIC_ALLOC)
    target_sources(opensomeip PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/freertos/memory.cpp)
endif()

# ThreadX platform sources (skipped when static allocation owns memory backend)
if(SOMEIP_USE_THREADX AND NOT SOMEIP_USE_STATIC_ALLOC)
    target_sources(opensomeip PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/threadx/memory.cpp)
endif()

# Static allocation platform sources
if(SOMEIP_USE_STATIC_ALLOC)
    target_sources(opensomeip PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/static/memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/static/buffer_pool.cpp)
    target_link_libraries(opensomeip PUBLIC etl::etl)

    # ETL safety configuration: disable exceptions, enable error logging.
    # The custom error handler (when registered) surfaces container overflow
    # as a return code rather than abort() — required for FMEA compliance.
    target_compile_definitions(opensomeip PUBLIC
        ETL_LOG_ERRORS=1
        ETL_THROW_EXCEPTIONS=0
    )

    # Malloc trap object library (linked into verification tests, not into main lib)
    add_library(someip_malloc_trap OBJECT
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/static/malloc_trap.cpp)
    target_compile_definitions(someip_malloc_trap PRIVATE SOMEIP_MALLOC_TRAP)
endif()

# Backward-compatible aliases so existing target_link_libraries() calls
# continue to work (e.g. target_link_libraries(my_app someip-rpc)).
add_library(someip-core ALIAS opensomeip)
add_library(someip-transport ALIAS opensomeip)
add_library(someip-rpc ALIAS opensomeip)
add_library(someip-sd ALIAS opensomeip)
add_library(someip-events ALIAS opensomeip)
add_library(someip-tp ALIAS opensomeip)
add_library(someip-serialization ALIAS opensomeip)
add_library(someip-e2e ALIAS opensomeip)
add_library(someip-common ALIAS opensomeip)

# Windows portability: suppress problematic macros from <windows.h>.
#   NOGDI            – prevents <wingdi.h> (defines ERROR as 0)
#   WIN32_LEAN_AND_MEAN – trims unused Win32 sub-headers
#   NOMINMAX         – prevents min/max macros that break std::min/std::max
if(WIN32)
    target_compile_definitions(opensomeip PRIVATE NOGDI WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Set library properties for proper soname versioning
set_target_properties(opensomeip PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# Symbol visibility for shared builds: hide by default, export explicitly
if(BUILD_SHARED_LIBS)
    set_target_properties(opensomeip PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()
