find_program(DOXYGEN_EXECUTABLE
  NAMES doxygen
  PATHS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\doxygen_is1;Inno Setup: App Path]/bin"
  /Applications/Doxygen.app/Contents/Resources
  /Applications/Doxygen.app/Contents/MacOS
  /usr/bin
  /usr/local/bin
  DOC "Doxygen documentation generation tool (http://www.doxygen.org)"
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Doxygen DEFAULT_MSG DOXYGEN_EXECUTABLE)

IF( Doxygen_FIND_COMPONENTS )
  FOREACH(comp ${Doxygen_FIND_COMPONENTS})
    if(${comp} MATCHES "Dot")
      find_program(DOXYGEN_DOT_EXECUTABLE
        NAMES dot
        PATHS "$ENV{ProgramFiles}/ATT/Graphviz/bin"
        "C:/Program Files/ATT/Graphviz/bin"
        "C:/Program Files/Graphviz 2.21/bin"
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\ATT\\Graphviz;InstallPath]/bin
        /Applications/Graphviz.app/Contents/MacOS
        /Applications/Doxygen.app/Contents/Resources
        /Applications/Doxygen.app/Contents/MacOS
        /usr/bin
        /us/local/bin
        DOC "Graphiz Dot tool for using Doxygen"
      )
      include(FindPackageHandleStandardArgs)
      find_package_handle_standard_args(Doxygen_Dot DEFAULT_MSG DOXYGEN_DOT_EXECUTABLE)
    ENDIF()
  ENDFOREACH()
ENDIF()

mark_as_advanced(
  DOXYGEN_EXECUTABLE
  DOXYGEN_DOT_EXECUTABLE
)

set_package_properties(Doxygen PROPERTIES
  URL "http://www.stack.nl/~dimitri/doxygen/"
  TYPE OPTIONAL
  PURPOSE "Generate API/library class documentation")
