# build examples
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# example setting global compilation flags
# set(CMAKE_C_FLAGS "-O0 -gdwarf-2 -gstrict-dwarf")

add_executable(read_accounting ${EXAMPLES_DIR}/read_accounting.c)


set(SOCK_FLAGS "-g")
set(SOCK_TESTER_SOURCES
  ${EXAMPLES_DIR}/socket_tester.c
)
set_source_files_properties(
  ${SOCK_TESTER_SOURCES}
  PROPERTIES COMPILE_FLAGS ${SOCK_FLAGS}
)
add_executable(socket_tester ${EXAMPLES_DIR}/socket_tester.c)
# set linking options for socket_tester
target_link_libraries(socket_tester res_api)

add_executable(small-big-write-sync ${EXAMPLES_DIR}/small-big-write-sync.c)
