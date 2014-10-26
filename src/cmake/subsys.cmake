# build module for generating kernel subsystems header file
# uses the scripts/find_subsystems.py
# author: Lucian Carata <lc525@cam.ac.uk>

#  ====================================================================
#
# SUBSYS_HEADER_GEN (public function)
#   L_ROOT = directory containing the linux source tree
#   L_VMLINUX = path towards a vmlinux that was built with CONFIG_DEBUG_INFO=y
#   L_BUILD = directory where vmlinux was originally built
#   OUT_LIST = name of header file to generate with list of subsystems
#   OUT_ADDR = name of header file to generate with kernel function
#              addresses for the given kernel binary, per subsystem
#   OUT_JSON = name of json file to generate with list of subsystems
#
#  ====================================================================
function(SUBSYS_HEADER_GEN L_ROOT L_VMLINUX L_BUILD OUT_LIST OUT_ADDR OUT_JSON)
  # Generate subsystems header files
  add_custom_command(
    OUTPUT ${OUT_JSON} ${OUT_LIST} ${OUT_ADDR}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_subsystems.py
    ARGS
      -l ${L_ROOT}
      -v ${L_VMLINUX}
      --build_dir ${L_BUILD}
      --find_subsystems
      -J ${OUT_JSON}
      --update_json
      --gen_shared_header ${OUT_LIST}
      > ${OUT_ADDR}
    COMMENT "Building kprobe address list from kernel binary, generating header files"
  )
  add_custom_target(subsys_gen ALL DEPENDS ${OUT_JSON} ${OUT_LIST} ${OUT_ADDR})
endfunction()
