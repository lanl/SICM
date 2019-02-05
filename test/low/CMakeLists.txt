add_executable(test1 test1.c)
add_executable(test2 test2.c)
add_executable(test3 test3.c)
add_executable(test4 test4.c)
add_executable(test5 test5.c)

# Just include the public SICM low-level interface
target_include_directories(test1 PUBLIC ../../include/public)
target_link_libraries(test1 PUBLIC sicm)
target_include_directories(test2 PUBLIC ../../include/public)
target_link_libraries(test2 PUBLIC sicm)
target_include_directories(test3 PUBLIC ../../include/public)
target_link_libraries(test3 PUBLIC sicm)
target_include_directories(test4 PUBLIC ../../include/public)
target_link_libraries(test4 PUBLIC sicm)
target_include_directories(test5 PUBLIC ../../include/public)
target_link_libraries(test5 PUBLIC sicm)

# Now actually run the tests
add_test(test1 test1)
