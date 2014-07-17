# global resourceful build file
# author: Lucian Carata <lc525@cam.ac.uk>
#
# Run cmake from a separate build directory (out of source build is required).
#
# sample build command line:
# [..build]$ cmake -DCMAKE_BUILD_TYPE=Release ..
# [..build]$ make
#
# or run the provided .build script within the build directory
#
cmake_minimum_required(VERSION 2.8)

# change the project name here by setting PNAME.
#
set(PNAME resourceful)

# General cmake project definition follows. Customize as necessary
project(${PNAME} C)

# variable definitions for generating configuration headers
set(PROJECT_MAJOR_VERSION 0)
set(PROJECT_MINOR_VERSION 1)
execute_process(
  COMMAND git --git-dir ${${PNAME}_SOURCE_DIR}/../.git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_REV
)
string(REPLACE "\n" "" GIT_REV ${GIT_REV})
set(PROJECT_PATCH_VERSION ${GIT_REV})



###
#
#  CMAKE MODULES
#
###
set(CMAKE_MODULE_PATH ${${PNAME}_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH ${${PNAME}_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
include(CMakeDependentOption)
include(FeatureSummary)
# custom modules
include(MacroOutOfSourceBuild)
include(InternalMacros)



###
#
# OPTIONS
#
###
set(CMAKE_C_FLAGS "-Werror")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
message("!! Build type: ${CMAKE_BUILD_TYPE}")

string( TOLOWER "${CMAKE_BUILD_TYPE}" bt_lower )
if(bt_lower STREQUAL "debug")
  set(DEFINE_NDEBUG)
else(bt_lower STREQUAL "debug")
  set(DEFINE_NDEBUG "\#define NDEBUG")
endif(bt_lower STREQUAL "debug")

###
#
# NO PROJECT DEPENDENCIES (YET)
#
###




###
#
# BUILD
#
###
ensure_out_of_source_build("${PNAME} requires an out of source build. ")

configure_project(
  ${PROJECT_MAJOR_VERSION}
  ${PROJECT_MINOR_VERSION}
  ${PROJECT_PATCH_VERSION}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/rscfl
)

set(GEN_INCLUDE_DIR ${PROJECT_BINARY_DIR}
  CACHE STRING "include for gen configs")


set (PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set (PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel)

# common includes and project settings
include_directories("${GEN_INCLUDE_DIR}") # for generated configs
include_directories("${PROJECT_INCLUDE_DIR}")

# build systemtap modules
include(stap)

set (stap_OUT_DIR ${PROJECT_BINARY_DIR}/Kstap_gen)
set (stap_MOD_NAME rscfl_${PROJECT_PATCH_VERSION})

set (stap_INCLUDES
  ${PROJECT_INCLUDE_DIR} # for cost.h and other headers
  ${PROJECT_BINARY_DIR}  # for generated config.h
)

set (stap_gen_SOURCES
  ${PROJECT_SOURCE_DIR}/resourceful.stp
)
set (stap_SOURCES
  ${PROJECT_SOURCE_DIR}/res_comm.c
  ${PROJECT_SOURCE_DIR}/stap_shim.c
  ${PROJECT_SOURCE_DIR}/probes.c
)

set (stap_DEPS # if any of those files change, the stap ko is rebuilt
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/stap_shim.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/probes.h
  ${PROJECT_INCLUDE_DIR}/rscfl/costs.h
  ${PROJECT_BINARY_DIR}/rscfl/config.h
  ${PROJECT_SOURCE_DIR}/resourceful.stp
)

file(MAKE_DIRECTORY ${stap_OUT_DIR})

# remember to pass variables that contain lists of files/directories with ""
stap_build(${stap_MOD_NAME}
  "${stap_INCLUDES}"
   ${stap_OUT_DIR}
   ${stap_gen_SOURCES}
   "${stap_SOURCES}"
   ${stap_DEPS}
)

# build examples
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)

# build user-facing library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

