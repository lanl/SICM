add_executable(stream stream.c)

# Use the compiler wrappers to compile it
SET(CMAKE_C_COMPILER "${CMAKE_SOURCE_DIR}/bin/compiler_wrapper.sh")
SET(CMAKE_AR "${CMAKE_SOURCE_DIR}/bin/ar_wrapper.sh")
SET(CMAKE_LINKER "${CMAKE_SOURCE_DIR}/bin/ld_wrapper.sh")
SET(CMAKE_RANLIB "${CMAKE_SOURCE_DIR}/bin/ranlib_wrapper.sh")

# Now actually run the tests
add_test(stream stream)
