##
# build tests
#
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(WITH_TESTS)

  set(TEST_LINK res_api ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} pthread)

  # Run sanity test first. This ensures rscfl is running.

  set (gtest_sanity_SOURCES
    ${TESTS_DIR}/gtest_sanity_check.cpp
  )
  lib_test(gtest_sanity_check "${gtest_sanity_SOURCES}" "${TEST_LINK}")

  set (api_test_SOURCES
    ${TESTS_DIR}/api_test.cpp
  )
  lib_test(api_test "${api_test_SOURCES}" "${TEST_LINK}")

  set (cycles_test_SOURCES
    ${TESTS_DIR}/cycles_test.cpp
  )
  lib_test(cycles_test "${cycles_test_SOURCES}" "${TEST_LINK}")

  set (sched_test_SOURCES
    ${TESTS_DIR}/sched_test.cpp
  )
  lib_test(sched_test "${sched_test_SOURCES}" "${TEST_LINK}")

  set (sendfile_test_SOURCES
    ${TESTS_DIR}/sendfile_test.cpp
  )
  lib_test(sendfile_test "${sendfile_test_SOURCES}" "${TEST_LINK}")

  if(SHDW_ENABLED)
    set (shdw_test_SOURCES
      ${TESTS_DIR}/shdw_test.cpp
    )
    lib_test(shdw_test "${shdw_test_SOURCES}" "${TEST_LINK}")
  endif(SHDW_ENABLED)

  set (socket_test_SOURCES
    ${TESTS_DIR}/socket_test.cpp
  )
  lib_test(socket_test "${socket_test_SOURCES}" "${TEST_LINK}")

  set (stress_test_SOURCES
    ${TESTS_DIR}/stress_test.cpp
  )
  lib_test(stress_test "${stress_test_SOURCES}" "${TEST_LINK}")

  set (wct_test_SOURCES
    ${TESTS_DIR}/wct_test.cpp
  )
  lib_test(wct_test "${wct_test_SOURCES}" "${TEST_LINK}")

endif(WITH_TESTS)
