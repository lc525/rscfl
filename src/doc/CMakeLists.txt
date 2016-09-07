## CMake documentation
#
# Defines the following custom targets:
#   docs    - generates the sphinx html output of the documentation
#
##

# configured documentation tools and intermediate build results
set(BINARY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/.sphinx_build")


if(WITH_DOCS)
  find_package(Sphinx REQUIRED)
  ## Sphinx Configuration

  if(NOT DEFINED SPHINX_THEME)
    set(SPHINX_THEME sphinx_rtd_theme)
  endif()
  if(NOT DEFINED SPHINX_THEME_DIR)
      set(SPHINX_THEME_DIR .themes)
  endif()

  # Sphinx cache with pickled ReST documents
  set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/.doctrees")
  # HTML output directory
  set(SPHINX_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")
  # Use the doxygen API documentation from path:
  set(SPHINX_DOXYGEN_PATH "${CMAKE_CURRENT_BINARY_DIR}/doxyxml")

  configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in"
      "${BINARY_BUILD_DIR}/conf.py"
      @ONLY)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/.static
            ${CMAKE_CURRENT_SOURCE_DIR}/.templates
            ${CMAKE_CURRENT_SOURCE_DIR}/.themes
            ${CMAKE_CURRENT_SOURCE_DIR}/.extensions
       DESTINATION ${BINARY_BUILD_DIR})

  add_custom_target(doc ALL
      ${SPHINX_EXECUTABLE}
      -q -b html
      -c "${BINARY_BUILD_DIR}"
      -d "${SPHINX_CACHE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${SPHINX_HTML_DIR}"
      COMMENT "Building HTML documentation with Sphinx ${CMAKE_CURRENT_SOURCE_DIR}")

   if("${CMAKE_GENERATOR}" MATCHES Make)
        install(CODE "execute_process(COMMAND ${CMAKE_BUILD_TOOL} doc WORKING_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}\")")
        install(DIRECTORY ${SPHINX_HTML_DIR}/ DESTINATION doc/${PROJECT_NAME})
   endif()
 endif(WITH_DOCS)
