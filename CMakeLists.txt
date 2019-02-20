cmake_minimum_required (VERSION 3.0)
project (SICM)

# #################################################
# https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/RPATH-handling
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")
# #################################################

# Use -DSICM_BUILD_HIGH_LEVEL=True to build the high-level interface.
set(SICM_BUILD_HIGH_LEVEL False CACHE BOOL "Should we build the high-level interface?")

# Install custom CMake modules for jemalloc and libnuma.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Compile and install the low- and high-level interfaces
add_subdirectory(src)

# Install the compiler wrappers if we're doing the high-level interface
if(SICM_BUILD_HIGH_LEVEL)
  add_subdirectory(bin)
endif()

# Compile some tests
enable_testing()
add_subdirectory(test)
