##
# build tests
#
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(WITH_TESTS)

  set(TEST_LINK res_api ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} pthread)

  set (gtest_sanity_SOURCES
    ${TESTS_DIR}/gtest_sanity_check.cpp
  )
  lib_test(gtest_sanity_check "${gtest_sanity_SOURCES}" "${TEST_LINK}")

endif(WITH_TESTS)
