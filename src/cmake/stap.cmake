# systemtap kernel module build file
# author: Lucian Carata <lc525@cam.ac.uk>

function(JOIN VALUES GLUE OUTPUT)
  string (REPLACE ";" "${GLUE}" _TMP_STR "${VALUES}")
  set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

#  ====================================================================
#
# STAP_BUILD (public function)
#   MOD_NAME = the name of resulting kernel module
#   INCLUDES = include files to add to kbuild make
#   OUT_DIR = the output directory for stap results
#   GEN_SRC = SystemTap .stp script
#   SRC = Other sources that need to be built for this module
#
#   Unnamed parameters:
#   ARGN = file dependencies (if one of those is modified the module
#          gets rebuilt)
#
#  ====================================================================
function(STAP_BUILD MOD_NAME INCLUDES OUT_DIR GEN_SRC SRC DEPS)
  # Generate includes list
  foreach(FIL ${INCLUDES})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    list(FIND _stap_include_path -I${ABS_FIL} _contains_already)
    if(${_contains_already} EQUAL -1)
      list(APPEND _stap_include_path -I${ABS_FIL})
    endif()
  endforeach()
  JOIN("${_stap_include_path}" " " _stap_includes)
  set(STAP_INCLUDES ${_stap_includes})

  if(NOT DEFINED DEFINE_NDEBUG)
    set(K_DBG "-g")
  endif()

  # Generate extra files list
  foreach(SRC_FIL ${SRC})
    get_filename_component(ABS_PATH ${SRC_FIL} ABSOLUTE)
    get_filename_component(FNAME ${ABS_PATH} NAME)
    get_filename_component(FNAME_NAME ${ABS_PATH} NAME_WE)
    add_custom_command(OUTPUT ${OUT_DIR}/${FNAME}
                       COMMAND ${CMAKE_COMMAND} -E copy ${ABS_PATH} ${OUT_DIR}
                       DEPENDS ${ABS_PATH}
                       COMMENT "Copying source file ${FNAME} for .ko")
    list(FIND _stap_objs ${FNAME_NAME}.o _contains_already)
    if(${_contains_already} EQUAL -1)
      list(APPEND _stap_objs ${FNAME_NAME}.o)
      list(APPEND _stap_objsfp ${OUT_DIR}/${FNAME_NAME}.o)
      list(APPEND _stap_srcf ${OUT_DIR}/${FNAME})
    endif()
  endforeach()
  JOIN("${_stap_objs}" " " STAP_O_FILES)

  find_path(STAP_RUNTIME runtime_defines.h
  PATHS /usr/local/share/systemtap/runtime
        /usr/share/systemtap/runtime
  NO_DEFAULT_PATH # ignore system paths
  )

  # Generate kernel module Makefile
  configure_file(
    "${PROJECT_SOURCE_DIR}/Makefile.in"
    "${OUT_DIR}/Makefile"
    @ONLY)

  get_filename_component(ABS_FIL ${GEN_SRC} ABSOLUTE)
  get_filename_component(NM_FIL ${GEN_SRC} NAME)

  # Generate stap sources
  add_custom_command(
    OUTPUT ${OUT_DIR}/${MOD_NAME}.c
    COMMAND stap
    ARGS
      -g                  # guru mode
      -p 3                # stop after code gen
      --tmpdir=${OUT_DIR} # temporary directory
      -k                  # keep temporary directory
      -m ${MOD_NAME}      # force module name
      -I ${PROJECT_SOURCE_DIR}/tapsets
      ${ABS_FIL}
      > /dev/null
    DEPENDS ${GEN_SRC} ${DEPS}
    COMMENT "Building stap kernel module ${MOD_NAME} from ${NM_FIL}"
  )
  add_custom_target(stap_gen ALL DEPENDS ${OUT_DIR}/${MOD_NAME}.c)


  # Build kernel module
  set( MODULE_TARGET_NAME rscfl_ko )
  set( MODULE_BIN_FILE    ${OUT_DIR}/${MOD_NAME}.ko )
  set( MODULE_OUTPUT_FILES    ${_stap_objsfp} )
  set( MODULE_SOURCE_DIR  ${OUT_DIR} )

  set( KERNEL_DIR "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" )
  set( KBUILD_CMD $(MAKE)
                  -C ${KERNEL_DIR}
                  M=${MODULE_SOURCE_DIR}
                  modules )

  add_custom_command( OUTPUT  ${MODULE_BIN_FILE}
                              ${MODULE_OUTPUT_FILES}
                      COMMAND ${KBUILD_CMD}
                      COMMAND cp -f ${MODULE_BIN_FILE} ${PROJECT_BINARY_DIR}
                      DEPENDS stap_gen ${_stap_srcf} ${DEPS}
                      COMMENT "Running kbuild for ${MODULE_BIN_FILE}"
                      VERBATIM )

  add_custom_target ( ${MODULE_TARGET_NAME} ALL
                      DEPENDS ${MODULE_BIN_FILE} stap_gen)

endfunction()
