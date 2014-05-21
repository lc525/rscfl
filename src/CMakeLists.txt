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

configure_project(
  ${PROJECT_MAJOR_VERSION}
  ${PROJECT_MINOR_VERSION}
  ${PROJECT_PATCH_VERSION}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(GEN_INCLUDE_DIR ${PROJECT_BINARY_DIR}
  CACHE STRING "include for gen configs")

# common includes and project settings
include_directories("${GEN_INCLUDE_DIR}") # for generated configs
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

set (PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set (PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel)


# build systemtap modules
include(stap)

set (stap_INCLUDES
  ${PROJECT_INCLUDE_DIR} # for cost.h and other headers
  ${PROJECT_SOURCE_DIR}  # for stap_shim.c
  ${PROJECT_BINARY_DIR}  # for generated config.h
)

set (stap_SOURCES
  ${PROJECT_SOURCE_DIR}/resourceful.stp
)

set (stap_OUT_DIR ${PROJECT_BINARY_DIR}/Kstap_gen)
file(MAKE_DIRECTORY ${stap_OUT_DIR})
stap_build(rscfl_${PROJECT_PATCH_VERSION}
           "${stap_INCLUDES}"
           ${stap_OUT_DIR}
           "${stap_SOURCES}"
)

# build userspace-kernelspace communication kernel module
# this module depends on the stap module being loaded into the kernel
set (res_comm_OUT_DIR ${PROJECT_BINARY_DIR}/Kres_comm)
file(MAKE_DIRECTORY ${res_comm_OUT_DIR})
Set( MODULE_OUTPUT_FILES    res_comm.o
                            res_comm.ko)

Set( MODULE_SOURCE_FILES    Kbuild
                            res_comm.c )
JOIN("${MODULE_SOURCE_FILES}" "," _msf)

Set( MODULE_FILE        res_comm.ko )
Set( MODULE_TARGET_NAME res_comm-module )
Set( MODULE_BIN_FILE    ${PROJECT_BINARY_DIR}/${MODULE_FILE} )
Set( MODULE_SOURCE_DIR  ${res_comm_OUT_DIR} )

Set( KERNEL_DIR "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" )
Set( KBUILD_CMD ${CMAKE_MAKE_PROGRAM}
                -C ${KERNEL_DIR}
                M=${MODULE_SOURCE_DIR}
                KBUILD_EXTRA_SYMBOLS=${stap_OUT_DIR}/Module.symvers
                modules )

Add_Custom_Command( OUTPUT  ${PROJECT_BINARY_DIR}/${MODULE_BIN_FILE}
                            ${MODULE_OUTPUT_FILES}
                    COMMAND cp -f ${PROJECT_SOURCE_DIR}/{${_msf}} ${res_comm_OUT_DIR}
                    COMMAND ${KBUILD_CMD}
                    COMMAND cp -f ${res_comm_OUT_DIR}/${MODULE_FILE} ${MODULE_BIN_FILE}
                    VERBATIM )

Add_Custom_Target ( ${MODULE_TARGET_NAME}
                    ALL
                    DEPENDS stap ${PROJECT_BINARY_DIR}/${MODULE_BIN_FILE} )

# build examples
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)

# build user-facing library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

