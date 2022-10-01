cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hyperrtls)

message("conf file: ${CONF_FILE}")
if (NOT ${CONF_FILE} MATCHES "prj_(.*)\.conf")
    message(FATAL_ERROR "Please specify build target")
endif()

set(BUILD_TARGET ${CMAKE_MATCH_1})
set(ALLOWED_BUILD_TYPES gw anchor tag)

if (NOT ${BUILD_TARGET} IN_LIST ALLOWED_BUILD_TYPES)
    message(FATAL_ERROR "Unknown build target: ${BUILD_TARGET}. Choose one of ${ALLOWED_BUILD_TYPES}")
endif()

message("Selected build target: ${BUILD_TARGET}")

set(sources_gw
    src/gw/main.c
    src/models/gw/gw_impl.c)

target_sources(app PRIVATE
               ${sources_${BUILD_TARGET}})
target_include_directories(app PRIVATE include)
