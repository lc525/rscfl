# build examples
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# example setting global compilation flags
# set(CMAKE_C_FLAGS "-O0 -gdwarf-2 -gstrict-dwarf")

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
