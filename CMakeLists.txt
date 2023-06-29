cmake_minimum_required(VERSION 3.13.1)

set(CMAKE_C_STANDARD 11)

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

file(GLOB_RECURSE sources_anchor_app src/apps/anchor/*.[ch])
file(GLOB_RECURSE sources_gw_app     src/apps/gw/*.[ch])
file(GLOB_RECURSE sources_tag_app    src/apps/tag/*.[ch])
file(GLOB_RECURSE sources_dw1000     src/dw1000/*.[ch])
file(GLOB_RECURSE sources_uwb        src/uwb/*.[ch])

set(sources_anchor
    "${sources_anchor_app}"
    "${sources_dw1000}"
    "${sources_uwb}")

set(sources_gw
    "${sources_gw_app}")

set(sources_tag
    "${sources_tag_app}"
    "${sources_dw1000}"
    "${sources_uwb}")

target_sources(app PRIVATE
               ${sources_${BUILD_TARGET}})
target_include_directories(app PRIVATE include)
