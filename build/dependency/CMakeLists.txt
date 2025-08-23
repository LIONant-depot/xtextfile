# The library
cmake_minimum_required(VERSION 3.10)

# Set policy CMP0169 to OLD to allow FetchContent_Populate
# FetchContent_MakeAvailable will automatically execute root cmakes which will break the build
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# Populate xerr
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xerr.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xerr"  # Adjusted to go up one level
)

FetchContent_GetProperties(xerr)
if(NOT xerr_POPULATED)
  message(STATUS "Populating xerr...")
  FetchContent_Populate(xerr)
  
  # Include xerr's CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xerr/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xerr")
endif()

#
# Define as an interface library only if not already defined
#
if(NOT TARGET xtextfile)

  # Define xbitmap as a proper static library
  add_library(xtextfile INTERFACE)

  # determine the root of our library
  if("${TARGET_PROJECT}" STREQUAL "xtextfile_unit_test")
    set(ROOT ".")
  else()
    set(ROOT "dependencies/xtextfile")

    # set the include path
    set(XTEXTFILE_INCLUDE_LIST "${ROOT}")

  endif()

  # set the files required
  set(XTEXTFILE_FILES_LIST
    "${ROOT}/source/xtextfile.h"
    "${ROOT}/source/xtextfile.cpp"
    "${ROOT}/README.md"
  )

  # Set global properties
  set_property(GLOBAL PROPERTY XTEXTFILE_GROUP "dependencies/xcore/xbitmap")
  set_property(GLOBAL PROPERTY XTEXTFILE_FILES "${XTEXTFILE_FILES_LIST}")
  set_property(GLOBAL PROPERTY XTEXTFILE_INCLUDES "${XTEXTFILE_INCLUDE_LIST}")
  set_property(GLOBAL APPEND PROPERTY COMPONENT_REGISTRY "XTEXTFILE")

endif()