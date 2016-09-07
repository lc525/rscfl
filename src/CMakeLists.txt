# ========================================
# Global Resourceful build file
# Author: Lucian Carata <lc525@cam.ac.uk>
# ========================================
#
# Run cmake from a separate build directory (out of source build is required).
# It's recommended you use the provided .build script within the build
# directory:
#
# [..build]$ ./.build
#
# If you want to run cmake manually, that is of course also possible
# (but check for what variables need setting before the command - the .build
# script provides the required hints);
#
# Sample build command line:
# [..]$ cd build
# [..build]$ cmake -DCMAKE_BUILD_TYPE=Release ..
# [..build]$ make
#
#
# To look at available optional compilation features, run (from the separate
# build dir):
#
# [..build]$ cmake -LH ..
#
# Highlights from option list, add -D[OPTION_NAME]=ON to cmake cmd to enable:
#   - WITH_DOCS          - build project documentation
#        default:   ON
#        requires:  python3
#                   sphinx (sphinx-doc.org)
#
#   - WITH_TESTS         - build the unit tests for each project. run them
#                          with "make check" after running make.
#        default:   ON
#        requires:  gtest (bundled with this project)
#        provides:  make target named "check"
#
#   - WITH_XEN_ENABLED   - build with support for gathering xen hypervisor
#                          scheduling data. the time the VM running rscfl
#                          has been scheduled out is integrated into subsystem
#                          measurements
#        default:   ON  (to avoid code rot)
#        requires:  (at runtime) - rscfl running on a Xen VM, with rscfl-enabled
#                                  Xen (private repo)
#
#   - WITH_SHDW_ENABLED  - build with support for shadow kernels. this adds
#                          functionality for creating/destroying shadow kernels
#                          and only setting measurement probes in some of the
#                          shadows.
#        default:   ON (to avoid code rot)
#        recommend: OFF (experimental feature)
#        requires:  (at runtime) - rscfl running on a Xen VM
#
# sample command line:
# [..build]$ cmake -DWITH_DOCS=ON ..
#
cmake_minimum_required(VERSION 2.8)

set(PNAME resourceful)
project(${PNAME} C CXX)

# variable definitions for generating configuration headers
# please use semantic versioning (semver.org)
# - all releases with the same major version need to be compatible API-wise
# - on publishing a given rscfl version, set PROJECT_TAG_VERSION to something
#   like "beta", "dev", "rel" (or leave it empty); leave it commented out to
#   use the current git revision hash (default)
set(PROJECT_MAJOR_VERSION 1)
set(PROJECT_MINOR_VERSION 1)
set(PROJECT_PATCH_VERSION 0)
#set(PROJECT_TAG_VERSION "rel")

# data layout version. libraries and kernel modules with different data
# layouts will not be able to communicate. this is not the same as MAJOR_VERSION
# because you can modify the interface in non-backwards compatible ways but
# still retain compatiblity with older rscfl kernel modules.
set(PROJECT_DATA_LAYOUT_VERSION 6)
# by default, set PROJECT_TAG_VERSION to the git revision
execute_process(
  COMMAND git --git-dir ${${PNAME}_SOURCE_DIR}/../.git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_REV
)
string(REPLACE "\n" "" GIT_REV ${GIT_REV})
if(RSCFL_BENCH EQUAL 1)
  set(PROJECT_TAG_VERSION "bench")
else()
  if(NOT DEFINED PROJECT_TAG_VERSION)
    set(PROJECT_TAG_VERSION ${GIT_REV})
  endif()
endif()

if(PROJECT_TAG_VERSION STREQUAL "rel")
  set(PVER ${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION})
else()
  set(PVER ${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION}-${PROJECT_TAG_VERSION})
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
set(DEFAULT_WITH_DOCS ON)
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

# enable this if running on top of an rscfl-enabled XEN hypervisor
option(WITH_XEN_ENABLED
  "Enable to get hypervisor-level scheduling data into rscfl. Only possible when running on rscfl-enabled Xen" ON)
# enable this if you want shadow kernels [experimental feature]
option(WITH_SHDW_ENABLED
  "Enable shadow kernel support [experimental]" ON)
option(WITH_DOCS
  "Build ${PNAME} documentation" ${DEFAULT_WITH_DOCS})

if(RSCFL_BENCH EQUAL 1)
  set(DEFINE_RSCFL_BENCH "\#define RSCFL_BENCH")
else()
  set(DEFINE_RSCFL_BENCH "//\#define RSCFL_BENCH")
endif()

if(WITH_XEN_ENABLED EQUAL OFF)
  message("-- [OPTION] Building without XEN support")
endif()
if(WITH_SHDW_ENABLED EQUAL OFF)
  message("-- [OPTION] Building without shadow kernel support")
endif()

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

if (WITH_DOCS)
  add_subdirectory(doc)
endif (WITH_DOCS)

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

# Uninstall target, place it here because others might want to set
# dependencies on it
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
               IMMEDIATE @ONLY)
add_custom_target(uninstall_files
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
add_custom_target(uninstall)

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
set(UPDATE_MODULES_UNINSTALL_AFTER uninstall_files)
include(update_modules)

set (rscfl_OUT_DIR ${PROJECT_BINARY_DIR}/Kbuild)
set (rscfl_MOD_NAME rscfl-${PVER})

set (rscfl_KINCLUDES
  ${PROJECT_INCLUDE_DIR} # for cost.h and other headers
  ${PROJECT_BINARY_DIR}  # for generated config.h
)

set (rscfl_KSOURCES
  ${PROJECT_COMMON_DIR}/res_common.c
  ${PROJECT_SOURCE_DIR}/acct.c
  ${PROJECT_SOURCE_DIR}/measurement/measurement.c
  ${PROJECT_SOURCE_DIR}/measurement/perf.c
  ${PROJECT_SOURCE_DIR}/priv_kallsyms.c
  ${PROJECT_SOURCE_DIR}/rscfl.c
  ${PROJECT_SOURCE_DIR}/shdw.c
  ${PROJECT_SOURCE_DIR}/subsys.c
  ${PROJECT_SOURCE_DIR}/chardev.c
  ${PROJECT_SOURCE_DIR}/cpu.c
  ${PROJECT_SOURCE_DIR}/kamprobes.c
  ${PROJECT_SOURCE_DIR}/probes.c
  ${PROJECT_SOURCE_DIR}/sched.c
  ${PROJECT_SOURCE_DIR}/xen.c
)

set (rscfl_KDEPS # if any of those files change, the stap ko is rebuilt
  subsys_gen
  ${PROJECT_BINARY_DIR}/rscfl/config.h
  ${PROJECT_INCLUDE_DIR}/rscfl/costs.h
  ${PROJECT_INCLUDE_DIR}/rscfl/res_common.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/acct.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/chardev.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/kamprobes.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/measurement.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/perf.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/priv_kallsyms.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/probes.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/rscfl.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/sched.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/subsys.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/shdw.h
  ${PROJECT_INCLUDE_DIR}/rscfl/kernel/xen.h
)

install(DIRECTORY include/rscfl/ DESTINATION include/rscfl)
install(FILES ${PROJECT_BINARY_DIR}/rscfl/config.h DESTINATION include/rscfl)

file(MAKE_DIRECTORY ${rscfl_OUT_DIR})

# remember to pass variables that contain lists of files/directories with ""
rscfl_kbuild(${rscfl_MOD_NAME}
  "${rscfl_KINCLUDES}"
   ${rscfl_OUT_DIR}
   "${rscfl_KSOURCES}"
   "${rscfl_KDEPS}"
)

# build user-facing library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

# build utility scripts
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scripts)

# add tests
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)

