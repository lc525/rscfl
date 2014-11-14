find_path(GTEST_INCLUDE_DIR gtest/gtest.h
    PATHS ${CMAKE_SOURCE_DIR}/common/external/gtest/include
          ${CMAKE_SOURCE_DIR}/external/gtest/include
    HINTS $ENV{GTEST_INCLUDE_DIR}
)

find_library(GTEST_LIBRARY
    NAMES gtest libgtest
    PATHS ${CMAKE_SOURCE_DIR}/common/external/gtest
          ${CMAKE_SOURCE_DIR}/external/gtest
    HINTS $ENV{GTEST_LIB_DIR}
    PATH_SUFFIXES lib build build-aux
    DOC "gtest: Google testing library"
)

find_library(GTEST_MAIN_LIBRARY
    NAMES gtest_main libgtest_main
    PATHS ${CMAKE_SOURCE_DIR}/common/external/gtest
          ${CMAKE_SOURCE_DIR}/external/gtest
    HINTS $ENV{GTEST_LIB_DIR}
    PATH_SUFFIXES lib build build-aux
    DOC "gtest: Google testing library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gtest DEFAULT_MSG
  GTEST_LIBRARY GTEST_MAIN_LIBRARY GTEST_INCLUDE_DIR)

mark_as_advanced(
  GTEST_INCLUDE_DIR
  GTEST_LIBRARY
  GTEST_MAIN_LIBRARY
)

set(GTEST_LIBRARIES ${GTEST_LIBRARY} CACHE STRING "gtest library")
set(GTEST_MAIN_LIBRARIES ${GTEST_MAIN_LIBRARY} CACHE STRING "gtest main library")
set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR} CACHE STRING "gtest include dir")

set_package_properties(gtest PROPERTIES
  URL "https://code.google.com/p/googletest/"
  TYPE OPTIONAL
  PURPOSE "for running unit tests - automatically built from source if not found")

