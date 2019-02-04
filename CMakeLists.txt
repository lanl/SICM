cmake_minimum_required (VERSION 3.0)
project (SICM)

# Install custom CMake modules for jemalloc and libnuma.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_subdirectory(src)
