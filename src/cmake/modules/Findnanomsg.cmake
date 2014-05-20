# Determine if nanomsg was downloaded locally within the project
#
set(NANOMSG_PATHS  ${CMAKE_SOURCE_DIR}/common/external/nanomsg/build-aux/lib
                 ${CMAKE_SOURCE_DIR}/external/nanomsg/build-aux/lib)

find_path(NANOMSG_INCLUDE_DIRECTORIES nanomsg/nn.h
  PATHS ${CMAKE_SOURCE_DIR}/common/external/nanomsg/build-aux/include
        ${CMAKE_SOURCE_DIR}/external/nanomsg/build-aux/include
  NO_DEFAULT_PATH # ignore system capnproto if installed
)

find_library(NANOMSG_LIBRARY
  NAMES nanomsg libnanomsg
  PATHS ${NANOMSG_PATHS}
  NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(nanomsg DEFAULT_MSG
  NANOMSG_LIBRARY
  NANOMSG_INCLUDE_DIRECTORIES
)

mark_as_advanced(
  NANOMSG_LIBRARY
  NANOMSG_INCLUDE_DIRECTORIES
)

if(NANOMSG_FOUND)
  set(NANOMSG_LOCAL_FOUND) # we have found capnproto locally within project path
                           # this means we have previously downloaded & built it
  list(APPEND NANOMSG_LIBRARIES ${NANOMSG_LIBRARY})
else(NANOMSG_FOUND)
  set(NANOMSG_PREFIX ${CMAKE_SOURCE_DIR}/external/nanomsg)
  set(NANOMSG_BUILD_PREFIX ${NANOMSG_PREFIX}/build-aux)
  MESSAGE(" \\-- Nanomsg will be downloaded and built in ${NANOMSG_BUILD_PREFIX}")
  ExternalProject_Add(nanomsg
    GIT_REPOSITORY https://github.com/nanomsg/nanomsg.git
    PREFIX ${NANOMSG_BUILD_PREFIX}
    GIT_TAG master
    UPDATE_COMMAND ${NANOMSG_PREFIX}/autogen.sh
    CONFIGURE_COMMAND ${NANOMSG_PREFIX}/configure --prefix=${NANOMSG_BUILD_PREFIX}
    BUILD_COMMAND make -s
    TMP_DIR ${CMAKE_SOURCE_DIR}/build/nanomsg
    STAMP_DIR ${CMAKE_SOURCE_DIR}/build/nanomsg
    DOWNLOAD_DIR ${NANOMSG_PREFIX}
    SOURCE_DIR ${NANOMSG_PREFIX}
    BINARY_DIR ${NANOMSG_BUILD_PREFIX}
  )
  ExternalProject_Add_Step(nanomsg prebuild
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NANOMSG_PREFIX}/build-aux
    COMMENT "Create nanomsg build directory"
    DEPENDEES download
    DEPENDERS configure
  )
  set(NANOMSG_LIBRARY "${NANOMSG_PREFIX}/build-aux/lib/libnanomsg.so")
  set(NANOMSG_INCLUDE_DIRECTORIES "${NANOMSG_PREFIX}/build-aux/include")
  mark_as_advanced(
    NANOMSG_LIBRARY
    NANOMSG_INCLUDE_DIRECTORIES
  )
  set(NANOMSG_FOUND)

endif(NANOMSG_FOUND)

set_package_properties(nanomsg PROPERTIES
  URL "https://github.com/nanomsg/nanomsg.git"
  TYPE REQUIRED
  PURPOSE "nanomsg socket library for communication patterns")

