find_path(RSCFL_INCLUDE_DIR rscfl/user/res_api.h
    PATHS /usr/include
          /usr/local/include
    HINTS $ENV{RSCFL_INCLUDE_DIR}
)

find_library(RSCFL_LIBRARY
    NAMES res_api libres_api
    PATHS /usr
          /usr/local
    HINTS $ENV{RSCFL_LIB_DIR}
    PATH_SUFFIXES lib
    DOC "rscfl: Resourceful user API"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(rscfl DEFAULT_MSG
  RSCFL_LIBRARY RSCFL_INCLUDE_DIR)

mark_as_advanced(
  RSCFL_INCLUDE_DIR
  RSCFL_LIBRARY
)

set(RSCFL_LIBRARIES ${RSCFL_LIBRARY} CACHE STRING "rscfl library")
set(RSCFL_INCLUDE_DIRS ${RSCFL_INCLUDE_DIR} CACHE STRING "rscfl include dir")

set_package_properties(rscfl PROPERTIES
  URL "https://github.com/lc525/rscfl"
  TYPE OPTIONAL
  PURPOSE "perform low-level resource consumption measurements")

