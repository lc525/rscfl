set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(libp_NAME res_api)

include_directories("${GEN_INCLUDE_DIR}") # for generated configs
include_directories("${PROJECT_INCLUDE_DIR}")

add_library(${libp_NAME} SHARED
  ${LIB_DIR}/res_api.c
  ${LIB_DIR}/../res_common.c)
add_dependencies(${libp_NAME} subsys_gen)
add_library(${libp_NAME}_static STATIC
  ${LIB_DIR}/res_api.c
  ${LIB_DIR}/../res_common.c)
add_dependencies(${libp_NAME}_static subsys_gen)

set_target_properties(${libp_NAME}_static PROPERTIES OUTPUT_NAME "res_api")
set_target_properties(${libp_NAME}_static PROPERTIES PREFIX "lib")

set_target_properties(${libp_NAME} PROPERTIES
  OUTPUT_NAME ${libp_NAME}
  VERSION ${VERSION}
  SOVERSION ${MAJOR_VERSION}
)

install(TARGETS ${libp_NAME} ${libp_NAME}_static
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
