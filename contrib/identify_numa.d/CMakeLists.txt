cmake_minimum_required (VERSION 3.0)

set(scripts
  optane
)

foreach(detector ${scripts})
  configure_file(${detector} ${detector} COPYONLY)
endforeach()

add_executable(knl knl.c)
target_link_libraries(knl ${NUMA_LIBRARY})

if (CUDAToolkit_FOUND)
    add_subdirectory(cuda)
endif()
