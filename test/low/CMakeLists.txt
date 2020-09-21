function(sicm_test src)
  get_filename_component(name "${src}" NAME_WE)
  add_executable("${name}" "${src}")

  target_include_directories("${name}" PUBLIC "${CMAKE_SOURCE_DIR}/include/low/public")
  target_link_libraries("${name}" PUBLIC sicm_SHARED)
  target_link_libraries("${name}" PRIVATE "${JEMALLOC_LDFLAGS}")
  add_test("${name}" "${name}")
endfunction()

foreach(NUM 1 2 3 4 5)
  sicm_test(test${NUM}.c)
endforeach()

sicm_test(allocator.cpp)
sicm_test(default_device.c)
