function(lib_project libp_NAME libp_SOURCES libp_HEADERS)

    if (NOT DEFINED GLOBAL_BUILD)

        include_directories("${CMAKE_SOURCE_DIR}/../common/include")
        include_directories("${CMAKE_SOURCE_DIR}/../common/external/gtest/include")

        set(GTEST_BUILD_DIR "${CMAKE_SOURCE_DIR}/../common/external/gtest/build")
        add_subdirectory("${CMAKE_SOURCE_DIR}/../common/external/gtest"
                         ${GTEST_BUILD_DIR})
        set(LINK_DIRECTORIES ${GTEST_BUILD_DIR})
        add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V)

    endif (NOT DEFINED GLOBAL_BUILD)

    add_library(${libp_NAME} SHARED ${libp_SOURCES})
    add_library(${libp_NAME}_static STATIC ${libp_SOURCES})

    install(TARGETS ${libp_NAME} ${libp_NAME}_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
    install(FILES ${libp_HEADERS} DESTINATION include/${libp_NAME})

endfunction(lib_project)



function(lib_test test_NAME test_SOURCES)
  if(WITH_TESTS)
    if (${ARGC} LESS 3)
      set(test_LINK pthread ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
    else()
        set(test_LINK ${ARGV2})
    endif (${ARGC} LESS 3)
    #MESSAGE(${test_LINK})

    find_package(libmongoc-1.0 1.7 REQUIRED)
    find_package(CURL REQUIRED)

    include_directories(${GTEST_INCLUDE_DIRS})

    set(TEST_FLAGS "-std=c++0x")
    add_executable(${test_NAME} ${test_SOURCES})
    set_source_files_properties(${test_SOURCES}
                                PROPERTIES COMPILE_FLAGS ${TEST_FLAGS})
#   target_link_libraries(${test_NAME} ${test_LINK})

    target_include_directories (${test_NAME} PRIVATE "${MONGOC_INCLUDE_DIRS};${CURL_INCLUDE_DIR}")
    target_link_libraries (${test_NAME} PRIVATE "${MONGOC_LIBRARIES};${CURL_LIBRARIES};${test_LINK}")
    target_compile_definitions (${test_NAME} PRIVATE "${MONGOC_DEFINITIONS}")
    
    add_test(ctest_build_${test_NAME} "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target ${test_NAME})
    add_test(run_${test_NAME} ${test_NAME} --gtest_color=yes --gtest_repeat=15 --gtest_output=xml:${test_NAME}_result.xml ${ARGV3})

    # wait for http://public.kitware.com/Bug/view.php?id=8438 to be solved
    # in order to clean this up properly (i.e do not use duplicate tests
    # to build dependencies and test binaries on make test/check:
    #
    #add_dependencies(test ${test_NAME})
    #add_dependencies(check ${test_NAME})
    if(NOT GTEST_FOUND)
      add_dependencies(${test_NAME} gtest)
      #set_tests_properties(ctest_build_${test_NAME} PROPERTIES DEPENDS gtest_external)
    endif()

    set_tests_properties(run_${test_NAME} PROPERTIES DEPENDS ctest_build_${test_NAME})
  endif(WITH_TESTS)
endfunction(lib_test)

macro(configure_project major minor patch tag_ver dlayout config_tpl_dir config_dir)

  set (MAJOR_VERSION ${major})
  set (MINOR_VERSION ${minor})
  set (PATCH_VERSION ${patch})
  set (DATA_LAYOUT_VERSION ${dlayout})
  set (TAG_VERSION ${tag_ver})
  if(${TAG_VERSION} STREQUAL "")
    set (VERSION ${major}.${minor}.${patch})
  else()
    set (VERSION ${major}.${minor}.${patch}-${tag_ver})
  endif()

  configure_file(
      "${config_tpl_dir}/config.h.in"
      "${config_dir}/config.h"
      @ONLY)

endmacro(configure_project)
