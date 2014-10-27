# build examples
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# example setting global compilation flags
# set(CMAKE_C_FLAGS "-O0 -gdwarf-2 -gstrict-dwarf")

##
# Read_accounting example
##
add_executable(read_accounting ${EXAMPLES_DIR}/read_accounting.c)
add_dependencies(read_accounting res_api subsys_gen)


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
add_dependencies(socket_tester subsys_gen)
# link with resourceful API
target_link_libraries(socket_tester res_api)

##
# Value Validator
#
set(VALIDATOR_FLAGS "-g")
set(
  VALIDATOR_SOURCES
  ${EXAMPLES_DIR}/value_validation.c
)
set_source_files_properties(
  ${VALIDATOR_SOURCES}
  PROPERTIES COMPILE_FLAGS ${VALIDATOR_FLAGS}
)
add_executable(value_validator ${VALIDATOR_SOURCES})
add_dependencies(value_validator subsys_gen)
target_link_libraries(value_validator res_api)

##
# Small writes example
##
add_executable(small-big-write-sync ${EXAMPLES_DIR}/small-big-write-sync.c)
