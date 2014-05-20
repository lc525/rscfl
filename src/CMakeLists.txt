# global resourceful build file
# author: Lucian Carata <lc525@cam.ac.uk>
#
# Run cmake from a separate build directory (out of source build is
# recommended).
#
# sample build command line:
# [..build]$ cmake -DCMAKE_BUILD_TYPE=Release ..
# [..build]$ make
#
# ..or run the provided .build script within the build directory
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
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
message("!! Build type: ${CMAKE_BUILD_TYPE}")



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

configure_project(${PROJECT_MAJOR_VERSION} ${PROJECT_MINOR_VERSION} ${PROJECT_PATCH_VERSION}
                 ${CMAKE_CURRENT_SOURCE_DIR}/include
                 ${CMAKE_CURRENT_BINARY_DIR})
set(GEN_INCLUDE_DIR ${PROJECT_BINARY_DIR} CACHE STRING "include for gen configs")

# common includes and project settings
include_directories("${GEN_INCLUDE_DIR}") # for generated configs
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

set (${PNAME}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set (${PNAME}_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel)


# build systemtap modules
include(stap)

set (stap_INCLUDES
  ${${PNAME}_INCLUDE_DIR}
  ${${PNAME}_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
  )

set (stap_SOURCES
  ${${PNAME}_SOURCE_DIR}/resourceful.stp
  )

set (stap_OUT_DIR ${PROJECT_BINARY_DIR}/stap_gen)
file(MAKE_DIRECTORY ${stap_OUT_DIR})
stap_build(rscfl_${PROJECT_PATCH_VERSION}
           "${stap_INCLUDES}"
           ${stap_OUT_DIR}
           "${stap_SOURCES}"
          )


# build examples
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)
add_executable(read_accounting ${EXAMPLES_DIR}/read_accounting.c)
#add_executable(small-big-write-sync ${EXAMPLES_DIR}/small-big-write-sync.c)
