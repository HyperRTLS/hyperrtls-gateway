cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hyperrtls)

# Logic borrowed from zephyr/cmake/modukes/configuration_files.cmake
string(REPLACE " " ";" CONF_FILE_AS_LIST "${CONF_FILE}")
list(GET CONF_FILE_AS_LIST 0 MAIN_CONF_FILE)

message("Detected conf file: ${MAIN_CONF_FILE}")
if (NOT ${MAIN_CONF_FILE} MATCHES "prj_(.*)\.conf")
    message(FATAL_ERROR "Please specify build target")
endif()

set(BUILD_TARGET ${CMAKE_MATCH_1})
set(ALLOWED_BUILD_TYPES gw anchor tag)

if (NOT ${BUILD_TARGET} IN_LIST ALLOWED_BUILD_TYPES)
    message(FATAL_ERROR "Unknown build target: ${BUILD_TARGET}. Choose one of ${ALLOWED_BUILD_TYPES}")
endif()

message("Selected build target: ${BUILD_TARGET}")

set(sources_gw
    src/apps/gw/mesh/handlers.c
    src/apps/gw/mesh/mesh.c
    src/apps/gw/mesh/mesh.h
    src/apps/gw/main.c
    src/apps/gw/main.h
    src/models/gw/gw_impl.c)

set(sources_tag
    src/apps/tag/main.c
    src/models/tag/tag_impl.c)

target_sources(app PRIVATE
               ${sources_${BUILD_TARGET}})
target_include_directories(app PRIVATE include)
