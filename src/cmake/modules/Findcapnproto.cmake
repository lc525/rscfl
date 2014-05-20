# Determine if capnproto was downloaded locally within the project
#
set(CAPNPROTO_PREFIX ${CMAKE_SOURCE_DIR}/external/capnproto)

set(CAPNP_PATHS  ${CMAKE_SOURCE_DIR}/common/external/capnproto/build-aux/c++
                 ${CMAKE_SOURCE_DIR}/external/capnproto/build-aux/c++)

find_path(CAPNP_INCLUDE_DIRECTORIES capnp/c++.capnp.h
  PATHS ${CMAKE_SOURCE_DIR}/common/external/capnproto/c++/src
        ${CMAKE_SOURCE_DIR}/external/capnproto/c++/src
  NO_DEFAULT_PATH # ignore system capnproto if installed
)

find_program(CAPNPC_EXECUTABLE capnp
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_program(CAPNPC_CXX_EXECUTABLE capnpc-c++
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_program(CAPNPC_CAPNP capnpc-capnp
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_library(CAPNP_RPC_LIBRARY
  NAMES capnp-rpc libcapnp-rpc
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_library(CAPNP_LIBRARY
  NAMES capnp libcapnp
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_library(CAPNPC_LIBRARY
  NAMES capnpc libcapnpc
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_library(KJ_ASYNC_LIBRARY
  NAMES kj-async libkj-async
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

find_library(KJ_LIBRARY
  NAMES kj libkj
  PATHS ${CAPNP_PATHS}
  NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(capnproto DEFAULT_MSG
  CAPNP_LIBRARY
  CAPNPC_EXECUTABLE
  CAPNP_INCLUDE_DIRECTORIES
  CAPNPC_CXX_EXECUTABLE
  CAPNPC_CAPNP
  CAPNP_RPC_LIBRARY
  CAPNPC_LIBRARY
  KJ_ASYNC_LIBRARY
  KJ_LIBRARY
)

mark_as_advanced(
  CAPNP_LIBRARY
  CAPNPC_EXECUTABLE
  CAPNP_INCLUDE_DIRECTORIES
  CAPNPC_CXX_EXECUTABLE
  CAPNPC_CAPNP
  CAPNP_RPC_LIBRARY
  CAPNPC_LIBRARY
  KJ_ASYNC_LIBRARY
  KJ_LIBRARY
)

if(CAPNPROTO_FOUND)
  set(CAPNPROTO_LOCAL_FOUND) # we have found capnproto locally within project path
                             # this means we have previously downloaded & built it
  list(APPEND CAPNP_LIBRARIES ${CAPNP_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${CAPNP_RPC_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${CAPNPC_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${KJ_ASYNC_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${KJ_LIBRARY})
else(CAPNPROTO_FOUND)
  MESSAGE(" \\-- Capnproto will be downloaded and built in ${CAPNPROTO_PREFIX}")
  ExternalProject_Add(capnproto
      GIT_REPOSITORY https://github.com/lc525/capnproto.git
      PREFIX ${CMAKE_SOURCE_DIR}/build/capnproto
      GIT_TAG cmakeable_local
      CMAKE_ARGS
          -DUSE_SUBMODULE_GTEST=ON
          -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
          -DGTEST_LIBRARY=${GTEST_LIBRARY}
          -DGTEST_MAIN_LIBRARY=${GTEST_MAIN_LIBRARY}
          -DGTEST_INCLUDE_DIRECTORIES=${GTEST_INCLUDE_DIRS}
      INSTALL_COMMAND ""
      UPDATE_COMMAND ""
      TMP_DIR ${CMAKE_SOURCE_DIR}/build/capnproto
      STAMP_DIR ${CMAKE_SOURCE_DIR}/build/capnproto
      DOWNLOAD_DIR ${CAPNPROTO_PREFIX}
      SOURCE_DIR ${CAPNPROTO_PREFIX}
      BINARY_DIR ${CAPNPROTO_PREFIX}/build-aux
  )
  ExternalProject_Add_Step(capnproto prebuild
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CAPNPROTO_PREFIX}/build-aux
    DEPENDEES download
    DEPENDERS configure
  )
  set(CAPNP_LIBRARY "${CAPNPROTO_PREFIX}/build-aux/c++/libcapnp.so")
  set(CAPNP_RPC_LIBRARY "${CAPNPROTO_PREFIX}/build-aux/c++/libcapnp-rpc.so")
  set(CAPNPC_LIBRARY "${CAPNPROTO_PREFIX}/build-aux/c++/libcapnpc.so")
  set(KJ_LIBRARY "${CAPNPROTO_PREFIX}/build-aux/c++/libkj.so")
  set(KJ_ASYNC_LIBRARY "${CAPNPROTO_PREFIX}/build-aux/c++/libkj-async.so")
  set(CAPNPC_EXECUTABLE "${CAPNPROTO_PREFIX}/build-aux/c++/capnp")
  set(CAPNPC_CXX_EXECUTABLE "${CAPNPROTO_PREFIX}/build-aux/c++/capnpc-c++")
  set(CAPNP_INCLUDE_DIRECTORIES "${CAPNPROTO_PREFIX}/c++/src")
  list(APPEND CAPNP_LIBRARIES ${CAPNP_RPC_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${CAPNP_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${CAPNPC_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${KJ_ASYNC_LIBRARY})
  list(APPEND CAPNP_LIBRARIES ${KJ_LIBRARY})
  if(NOT GTEST_FOUND)
    add_dependencies(capnproto gtest)
  endif(NOT GTEST_FOUND)
  set(CAPNPROTO_FOUND)
endif(CAPNPROTO_FOUND)

set_package_properties(capnproto PROPERTIES
  URL "https://github.com/kentov/capnproto"
  TYPE REQUIRED
  PURPOSE "capnproto serialization/RPC system")

