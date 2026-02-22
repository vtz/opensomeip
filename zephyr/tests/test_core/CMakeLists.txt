cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_core)

set(SOMEIP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

target_sources(app PRIVATE src/main.cpp)

list(APPEND ZEPHYR_EXTRA_MODULES ${SOMEIP_ROOT})
