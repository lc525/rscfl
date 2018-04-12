#build scripts
set(SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# example setting global compilation flags
# set(CMAKE_C_FLAGS "-O0 -gdwarf-2 -gstrict-dwarf")

##
# Module shutdown script
#

find_package(libmongoc-1.0 1.7 REQUIRED)
find_package(CURL REQUIRED)

set(RSCFL_STOP_FLAGS "-Werror")
set(
 RSCFL_STOP_SOURCES
 ${SCRIPTS_DIR}/rscfl_stop.c
)
set_source_files_properties(
 ${RSCFL_STOP_SOURCES}
 PROPERTIES COMPILE_FLAGS ${RSCFL_STOP_FLAGS}
)
add_executable(rscfl_stop ${RSCFL_STOP_SOURCES})
add_dependencies(rscfl_stop subsys_gen)
#target_link_libraries(rscfl_stop res_api)
target_include_directories (rscfl_stop PRIVATE "${MONGOC_INCLUDE_DIRS};${CURL_INCLUDE_DIR}")
target_link_libraries (rscfl_stop PRIVATE "${MONGOC_LIBRARIES};${CURL_LIBRARIES}" res_api)
target_compile_definitions (rscfl_stop PRIVATE "${MONGOC_DEFINITIONS}")



install(TARGETS rscfl_stop
  RUNTIME DESTINATION bin
)
