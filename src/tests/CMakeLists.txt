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

  set (socket_test_SOURCES
    ${TESTS_DIR}/socket_test.cpp
  )
  lib_test(socket_test "${socket_test_SOURCES}" "${TEST_LINK}")

  set (sendfile_test_SOURCES
    ${TESTS_DIR}/sendfile_test.cpp
  )
  lib_test(sendfile_test "${sendfile_test_SOURCES}" "${TEST_LINK}")

  set (stress_test_SOURCES
    ${TESTS_DIR}/stress_test.cpp
  )
  lib_test(stress_test "${stress_test_SOURCES}" "${TEST_LINK}")

endif(WITH_TESTS)
