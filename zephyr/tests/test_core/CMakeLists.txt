cmake_minimum_required(VERSION 3.20.0)

set(SOMEIP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${SOMEIP_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_core)

target_sources(app PRIVATE src/main.cpp)
