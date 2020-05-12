cmake_minimum_required (VERSION 3.0)

add_subdirectory(memory_characterization)

configure_file(sicm.pc sicm.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sicm.pc DESTINATION lib/pkgconfig)

add_executable(list_numa_nodes list_numa_nodes.c)
target_link_libraries(list_numa_nodes ${NUMA_LIBRARY})

configure_file(identify_numa identify_numa COPYONLY)
add_subdirectory(identify_numa.d)
