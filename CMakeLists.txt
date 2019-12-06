cmake_minimum_required (VERSION 3.0)
project (SICM)

# Use -DSICM_BUILD_HIGH_LEVEL=True to build the high-level interface.
set(SICM_BUILD_HIGH_LEVEL False CACHE BOOL "Should we build the high-level interface?")

find_package(Threads REQUIRED)
link_libraries(Threads::Threads m dl)

# Install custom CMake modules for jemalloc and libnuma.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# make sure jemalloc is found
find_package(Jemalloc REQUIRED)
# if JEMALLOC_ROOT_DIR was not provided
if ((NOT JEMALLOC_ROOT_DIR) AND JEMALLOC_LIBRARIES)
  get_filename_component(JEMALLOC_ROOT_DIR ${JEMALLOC_LIBRARIES} ABSOLUTE) # ${JEMALLOC_ROOT_DIR}/lib/libjemalloc.so
  get_filename_component(JEMALLOC_ROOT_DIR ${JEMALLOC_ROOT_DIR} DIRECTORY) # ${JEMALLOC_ROOT_DIR}/lib
  get_filename_component(JEMALLOC_ROOT_DIR ${JEMALLOC_ROOT_DIR} DIRECTORY) # ${JEMALLOC_ROOT_DIR}
endif()

# Compile and install the low- and high-level interfaces
add_subdirectory(src)

# Install the compiler wrappers if we're doing the high-level interface
if(SICM_BUILD_HIGH_LEVEL)
  add_subdirectory(bin)
endif()

# Add helper code found in the contrib directory
add_subdirectory(contrib)

# Compile some tests
enable_testing()
add_subdirectory(test)
