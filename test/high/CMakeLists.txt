add_executable(stream stream.c)

# Use the compiler wrappers to compile it
SET(CMAKE_C_COMPILER "compiler_wrapper.sh")
SET(CMAKE_AR "ar_wrapper.sh")
SET(CMAKE_LINKER "ld_wrapper.sh")
SET(CMAKE_RANLIB "ranlib_wrapper.sh")

# Now actually run the tests
add_test(stream stream)
