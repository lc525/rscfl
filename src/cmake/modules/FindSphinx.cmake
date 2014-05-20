find_program(SPHINX_EXECUTABLE
    NAMES sphinx-build sphinx-build2
    HINTS
    $ENV{SPHINX_DIR}
    PATH_SUFFIXES bin
    DOC "Sphinx documentation generator"
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sphinx DEFAULT_MSG SPHINX_EXECUTABLE)

mark_as_advanced(
  SPHINX_EXECUTABLE
)

set_package_properties(Sphinx PROPERTIES
  URL "http://sphinx-doc.org/"
  TYPE OPTIONAL
  PURPOSE "Generate html manuals and design docs")
