function(sicm_test src)
  get_filename_component(name "${src}" NAME_WE)
  add_executable("${name}" "${src}")

  target_include_directories("${name}" PUBLIC "${CMAKE_SOURCE_DIR}/include/low/public")
  target_link_libraries("${name}" PUBLIC sicm_SHARED)
  target_link_libraries("${name}" PRIVATE "${JEMALLOC_LDFLAGS}")
  add_test("${name}" "${name}")
endfunction()

foreach(NUM 1 2)
  sicm_test(test${NUM}.c)
endforeach()

option(HUGE_ALLOC_TESTS "Add tests that allocate 8GB+ memory" ON)
if (HUGE_ALLOC_TESTS)
  foreach(NUM 3 4 5)
    sicm_test(test${NUM}.c)
  endforeach()
endif()

sicm_test(default_device.c)

add_test(allocator ${CMAKE_BINARY_DIR}/examples/low/allocators)
