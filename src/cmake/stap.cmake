# inspired by google's protobuf cmake module
#  ====================================================================
#
# STAP_BUILD (public function)
#   MOD_NAME = the name of resulting kernel module
#   INCLUDE_FILES = include files to add to kbuild make
#   OUT_DIR = the output directory for stap results
#   ARGN = stp files
#
#  ====================================================================

function(JOIN VALUES GLUE OUTPUT)
  string (REPLACE ";" "${GLUE}" _TMP_STR "${VALUES}")
  set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

function(STAP_BUILD MOD_NAME INCLUDE_FILES OUT_DIR)
  if(NOT ARGN)
    message(SEND_ERROR "Error: STAP_BUILD() called without any .stp files")
    return()
  endif(NOT ARGN)

  # Create an include path for each file specified

  foreach(FIL ${INCLUDE_FILES})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    list(FIND _stap_include_path ${ABS_FIL} _contains_already)
    if(${_contains_already} EQUAL -1)
      list(APPEND _stap_include_path ${ABS_FIL})
    endif()
  endforeach()

  add_custom_target(stap ALL)
  JOIN("${_stap_include_path}" ":" _stap_includes)
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)

    add_custom_command(
      TARGET stap
      COMMAND stap
      ARGS -g -p 4 -k -B C_INCLUDE_PATH="${_stap_includes}" --tmpdir=${OUT_DIR} -m ${MOD_NAME} ${ABS_FIL}
      DEPENDS ${ABS_FIL}
      COMMENT "Building stap kernel module ${MOD_NAME} from ${FIL}"
      )
  endforeach()


endfunction()
