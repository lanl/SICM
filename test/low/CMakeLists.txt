add_executable(test1 test1.c)

# Just include the public SICM low-level interface
target_include_directories(test1 PUBLIC ../../include/public)
target_link_libraries(test1 PUBLIC sicm)
