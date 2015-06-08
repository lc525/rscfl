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

set(PNAME resourceful)
project(${PNAME} C CXX)

# variable definitions for generating configuration headers
# please use semantic versioning (semver.org)
# - all releases with the same major version need to be compatible API-wise
# - set PROJECT_TAG_VERSION to "" if you want no tags (the variable is needed)
set(PROJECT_MAJOR_VERSION 0)
set(PROJECT_MINOR_VERSION 7)
set(PROJECT_PATCH_VERSION 0)
# data layout version. libraries and kernel modules with different data
# layouts will not be able to communicate. this is not the same as MAJOR_VERSION
# because you can modify the interface in non-backwards compatible ways but
# still retain compatiblity with older rscfl kernel modules.
set(PROJECT_DATA_LAYOUT_VERSION 3)
# by default, set PROJECT_TAG_VERSION to the git revision
execute_process(
  COMMAND git --git-dir ${${PNAME}_SOURCE_DIR}/../.git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_REV
)
string(REPLACE "\n" "" GIT_REV ${GIT_REV})
# set tag version to something like "beta", "dev", "rel" (or leave it empty)
# on publishing a given rscfl version
#set(PROJECT_TAG_VERSION "rel")
set(PROJECT_TAG_VERSION ${GIT_REV})

if(${PROJECT_TAG_VERSION} STREQUAL "")
  set(PVER ${PROJECT_MAJOR_VERSION}_${PROJECT_MINOR_VERSION}_${PROJECT_PATCH_VERSION})
else()
  set(PVER ${PROJECT_MAJOR_VERSION}_${PROJECT_MINOR_VERSION}_${PROJECT_PATCH_VERSION}_${PROJECT_TAG_VERSION})
endif()

# what to do on API version mismatch (the .so is a different version from
# tha API)
set(ERR_ON_VERSION_MISMATCH True)

if(DEFINED ERR_ON_VERSION_MISMATCH)
  set(DEFINE_VERSION_MISMATCH_POLICY "\#define RSCFL_ERR_VER_MISMATCH")
else()
  set(DEFINE_VERSION_MISMATCH_POLICY "//\#define RSCFL_ERR_VER_MISMATCH")
endif()

# set default project build options:
set(DEFAULT_WITH_TESTS ON)


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

set(LINUX_ROOT $ENV{RSCFL_LINUX_ROOT}
  CACHE PATH "A linux source directory for subsystem identification")
set(LINUX_BUILD $ENV{RSCFL_LINUX_BUILD}
  CACHE PATH "The linux directory in which vmlinux was originally built")
set(LINUX_VMLINUX $ENV{RSCFL_LINUX_VMLINUX}
  CACHE PATH "The path towards a vmlinux that was built with CONFIG_DEBUG_INFO=y")

#Google Testing Framework options
# the options are mutually exclusive and are listed in order of preference,
# but if Gtest is not found using the given option the other ones will be
# attempted in turn.
option(USE_PROJECT_GTEST # recommended, automatic fallthrough
  "Use the local Gtest, if found within project subdirectories" ON)
option(USE_REPO_GTEST
  "Download and build gtest from online repository during make" OFF)
#option(USE_SYSTEM_GTEST
#  "Attempt using the system-wide Gtest installation" OFF)
option(WITH_TESTS
  "Build ${PNAME} tests" ${DEFAULT_WITH_TESTS})

###
#
# EXTERNAL DEPENDENCIES
#
###
include(ExternalProject)

# Google Testing Framework
set(FALLTHROUGH "0")
if(USE_PROJECT_GTEST)
  find_package(gtest)
  if(NOT GTEST_FOUND)
    set(FALLTHROUGH "1")
  endif()
endif()
if(USE_REPO_GTEST OR FALLTHROUGH STREQUAL "1")
  set(FALLTHROUGH "0")
  set(GTEST_PREFIX ${CMAKE_SOURCE_DIR}/external/gtest)
  MESSAGE(" \\-- Gtest will be downloaded and built in ${GTEST_PREFIX}")
  ExternalProject_Add(
      gtest
      PREFIX ${CMAKE_BINARY_DIR}/gtest
      GIT_REPOSITORY https://github.com/lc525/gtest.git
      GIT_TAG release-1.7.0
      CMAKE_ARGS
          -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} -std=c++0x
          -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      INSTALL_COMMAND ""
      UPDATE_COMMAND ""
      TMP_DIR ${CMAKE_BINARY_DIR}/gtest
      STAMP_DIR ${CMAKE_BINARY_DIR}/gtest
      DOWNLOAD_DIR ${GTEST_PREFIX}
      SOURCE_DIR ${GTEST_PREFIX}
      BINARY_DIR ${GTEST_PREFIX}/build-aux
      #INSTALL_DIR ${CMAKE_BINARY_DIR}/install
  )
  set(GTEST_LIBRARY "${GTEST_PREFIX}/build-aux/libgtest.a")
  set(GTEST_MAIN_LIBRARY "${GTEST_PREFIX}/build-aux/libgtest_main.a")
  set(GTEST_INCLUDE_DIR "${GTEST_PREFIX}/include")

  set(GTEST_LIBRARIES ${GTEST_LIBRARY})
  set(GTEST_MAIN_LIBRARIES ${GTEST_MAIN_LIBRARY})
  set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
endif()
#  if(USE_SYSTEM_GTEST)
#endif()

if(WITH_TESTS)
  add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V)
  enable_testing()
endif(WITH_TESTS)



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
  ${PROJECT_TAG_VERSION}
  ${PROJECT_DATA_LAYOUT_VERSION}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/rscfl
)

set(GEN_INCLUDE_DIR ${PROJECT_BINARY_DIR}
  CACHE STRING "include for gen configs")


set (PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set (PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel)
set (PROJECT_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# common includes and project settings
include_directories("${GEN_INCLUDE_DIR}") # for generated configs
include_directories("${PROJECT_INCLUDE_DIR}")

# build list of subsystems
#
if(DEFINED ${SUBSYS_OPT_ARG})
  set(SUBSYS_OPT_ARG "")
endif(DEFINED ${SUBSYS_OPT_ARG})
# do not depend on any of those generated files directly, but on
# the subsys_gen build target
include(subsys)
subsys_header_gen(${LINUX_ROOT}
  ${LINUX_VMLINUX}
  ${LINUX_BUILD}
  ${PROJECT_INCLUDE_DIR}/rscfl/subsys_list.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/subsys_addr.h
  ${PROJECT_BINARY_DIR}/subsys_list.json
  ${SUBSYS_OPT_ARG}
)

# build systemtap modules
include(rscfl_kbuild)

set (rscfl_OUT_DIR ${PROJECT_BINARY_DIR}/Kbuild)
set (rscfl_MOD_NAME rscfl_${PVER})

set (rscfl_KINCLUDES
  ${PROJECT_INCLUDE_DIR} # for cost.h and other headers
  ${PROJECT_BINARY_DIR}  # for generated config.h
)

set (rscfl_KSOURCES
  ${PROJECT_COMMON_DIR}/res_common.c
  ${PROJECT_SOURCE_DIR}/acct.c
  ${PROJECT_SOURCE_DIR}/counters/measurement.c
  ${PROJECT_SOURCE_DIR}/counters/perf.c
  ${PROJECT_SOURCE_DIR}/PMurHash.c
  ${PROJECT_SOURCE_DIR}/priv_kallsyms.c
  ${PROJECT_SOURCE_DIR}/rscfl.c
  ${PROJECT_SOURCE_DIR}/tapsets/chardev.c
  ${PROJECT_SOURCE_DIR}/tapsets/cpu.c
  ${PROJECT_SOURCE_DIR}/tapsets/hasht.c
  ${PROJECT_SOURCE_DIR}/tapsets/kamprobes.c
  ${PROJECT_SOURCE_DIR}/tapsets/probes.c
  ${PROJECT_SOURCE_DIR}/tapsets/sched.c
)

set (rscfl_KDEPS # if any of those files change, the stap ko is rebuilt
  subsys_gen
  ${PROJECT_BINARY_DIR}/rscfl/config.h
  ${PROJECT_INCLUDE_DIR}/rscfl/costs.h
  ${PROJECT_INCLUDE_DIR}/rscfl/PMurHash.h
  ${PROJECT_INCLUDE_DIR}/rscfl/res_common.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/acct.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/chardev.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/hasht.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/kamprobes.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/measurement.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/perf.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/priv_kallsyms.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/probes.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/sched.h
)

file(MAKE_DIRECTORY ${rscfl_OUT_DIR})

# remember to pass variables that contain lists of files/directories with ""
rscfl_kbuild(${rscfl_MOD_NAME}
  "${rscfl_KINCLUDES}"
   ${rscfl_OUT_DIR}
   "${rscfl_KSOURCES}"
   "${rscfl_KDEPS}"
)

# build examples
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)

# build user-facing library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

# add tests
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
