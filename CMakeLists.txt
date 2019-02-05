cmake_minimum_required (VERSION 3.0)
project (SICM)

# Use -DSICM_BUILD_HIGH_LEVEL=True to build the high-level interface.
set(SICM_BUILD_HIGH_LEVEL False CACHE BOOL "Should we build the high-level interface?")

# Install custom CMake modules for jemalloc and libnuma.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Compile the low- and high-level interfaces
add_subdirectory(src)

# Compile some tests
enable_testing()
add_subdirectory(test)
