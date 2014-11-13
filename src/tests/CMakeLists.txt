##
# build tests
#
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(WITH_TESTS)

  set(TEST_FLAGS "-std=c++0x")
  set(TEST_LINK res_api ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} pthread)

  set (gtest_sanity_SOURCES
    ${TESTS_DIR}/gtest_sanity_check.cpp
  )
  set_source_files_properties(${gtest_sanity_SOURCES}
                              PROPERTIES COMPILE_FLAGS ${TEST_FLAGS})
  lib_test(gtest_sanity_check "${gtest_sanity_SOURCES}" "${TEST_LINK}")

  set (socket_test_SOURCES
    ${TESTS_DIR}/socket_test.cpp
  )
  set_source_files_properties(${socket_test_SOURCES}
                              PROPERTIES COMPILE_FLAGS ${TEST_FLAGS})
  lib_test(socket_test "${socket_test_SOURCES}" "${TEST_LINK}")



endif(WITH_TESTS)
