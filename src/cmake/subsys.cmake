# build module for generating kernel subsystems header file
# uses the scripts/find_subsystems.py
# author: Lucian Carata <lc525@cam.ac.uk>

#  ====================================================================
#
# SUBSYS_HEADER_GEN (public function)
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
function(SUBSYS_HEADER_GEN L_ROOT L_BUILD OUT_LIST OUT_ADDR OUT_JSON)
  # Generate subsystems header files
  add_custom_command(
    OUTPUT ${OUT_JSON} ${OUT_LIST} ${OUT_ADDR}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_subsystems.py
    ARGS
      -l ${L_ROOT}
      --build_dir ${L_BUILD}
      --find_subsystems
      -J ${OUT_JSON}
      --update_json
      --gen_shared_header ${OUT_LIST}
      > ${OUT_ADDR}
    COMMENT "Building kprobe address list from kernel binary, generating header files"
  )
  add_custom_target(subsys_gen ALL DEPENDS ${OUTPUT})
endfunction()
