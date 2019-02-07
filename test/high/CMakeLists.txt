add_executable(stream mysecond.c stream.c)

# Just include the public SICM high-level interface
target_include_directories(stream PUBLIC ../../include/public)
target_link_libraries(stream PUBLIC sicm_high)

# Now actually run the tests
add_test(stream stream)
