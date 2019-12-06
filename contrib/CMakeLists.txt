cmake_minimum_required (VERSION 3.0)

add_subdirectory(memory_characterization)

configure_file(sicm.pc sicm.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sicm.pc DESTINATION lib/pkgconfig)
