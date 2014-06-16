# build examples
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# example setting global compilation flags
# set(CMAKE_C_FLAGS "-O0 -gdwarf-2 -gstrict-dwarf")

##
# Read_accounting example
##
add_executable(read_accounting ${EXAMPLES_DIR}/read_accounting.c)


##
# Socket example
##
set(SOCK_FLAGS "-g")
set(
  SOCK_TESTER_SOURCES
  ${EXAMPLES_DIR}/socket_tester.c
)
set_source_files_properties(
  ${SOCK_TESTER_SOURCES}
  PROPERTIES COMPILE_FLAGS ${SOCK_FLAGS}
)
add_executable(socket_tester ${SOCK_TESTER_SOURCES})
# link with resourceful API
target_link_libraries(socket_tester res_api)


##
# Small writes example
##
add_executable(small-big-write-sync ${EXAMPLES_DIR}/small-big-write-sync.c)
