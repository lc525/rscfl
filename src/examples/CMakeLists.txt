find_package (libmongoc-1.0 1.7 REQUIRED)
find_package (CURL REQUIRED)

add_executable (bran_example ../bran_example.c)
target_include_directories (bran_example PRIVATE "${MONGOC_INCLUDE_DIRS};${CURL_INCLUDE_DIR}")
target_link_libraries (bran_example PRIVATE "${MONGOC_LIBRARIES};${CURL_LIBRARIES}" res_api)
target_compile_definitions (bran_example PRIVATE "${MONGOC_DEFINITIONS}")
