set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(res_api SHARED ${LIB_DIR}/res_api.c)
add_library(res_api_static STATIC ${LIB_DIR}/res_api.c)

SET_TARGET_PROPERTIES(res_api_static PROPERTIES OUTPUT_NAME "res_api")
SET_TARGET_PROPERTIES(res_api_static PROPERTIES PREFIX "lib")
