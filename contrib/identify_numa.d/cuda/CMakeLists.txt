cmake_minimum_required (VERSION 3.0)

set(scripts
  numa_status
  smi-topo-m
  smi-xq
)

foreach(file ${scripts})
    configure_file(${file} ${file} COPYONLY)
endforeach()

add_executable(hwloc hwloc.c)
target_include_directories(hwloc PRIVATE ${CUDAToolkit_INCLUDE_DIRS} ${HWLOC_INCLUDE_DIRS})
target_link_libraries(hwloc ${CUDAToolkit_LDFLAGS} ${HWLOC_LDFLAGS})

