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
  execute_process(
    COMMAND uname -r
    OUTPUT_VARIABLE KERNEL_RELEASE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  get_filename_component(OUT_JSON_DIR ${OUT_JSON} DIRECTORY)
  set(K_DEP_FILE ${OUT_JSON_DIR}/subsys_for_${KERNEL_RELEASE}.json)

  if(${ARGC} GREATER 6)
    set(OPT_SUB_ARG ${ARGN})
  endif(${ARGC} GREATER 6)
  
  # Generate subsystems header files
  add_custom_command(
    OUTPUT ${OUT_JSON} ${OUT_LIST} ${OUT_ADDR} ${K_DEP_FILE}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_subsystems.py
     ARGS
      -l ${L_ROOT}
      -v ${L_VMLINUX}
      --build_dir ${L_BUILD}
      --find_subsystems
      -J ${OUT_JSON}
      --update_json
      --gen_shared_header ${OUT_LIST}
      ${OPT_SUB_ARG}
      > ${OUT_ADDR}
    COMMAND touch ARGS ${K_DEP_FILE}
    COMMENT "Building kprobe address list from kernel binary, generating header files"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_subsystems.py
  )
  add_custom_target(subsys_gen DEPENDS
    ${OUT_JSON}
    ${OUT_LIST}
    ${OUT_ADDR}
    ${K_DEP_FILE}
    )
endfunction()
