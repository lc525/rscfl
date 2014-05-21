set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(res_api SHARED ${LIB_DIR}/res_api.c)
add_library(res_api_static STATIC ${LIB_DIR}/res_api.c)
