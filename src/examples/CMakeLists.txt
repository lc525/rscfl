# build examples
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(read_accounting ${EXAMPLES_DIR}/read_accounting.c)
add_executable(socket_tester ${EXAMPLES_DIR}/socket_tester.c)
add_executable(small-big-write-sync ${EXAMPLES_DIR}/small-big-write-sync.c)
