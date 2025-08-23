include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

FetchAndPopulate("https://github.com/LIONant-depot/xerr.git")

DefineInterfaceComponent(xtextfile "dependencies/xcore"
  "source/xtextfile.h"
  "source/xtextfile.cpp"
  "README.md"
)