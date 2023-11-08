set(DETECTORS
  x86.c
  powerpc.c
  DRAM.c
)

if (hip_FOUND)
  list(APPEND DETECTORS HIP.c)
endif()

if (IntelDPCPP_FOUND)
  list(APPEND DETECTORS sycl.cpp)
endif()

foreach(type ${TYPES})
  foreach(detector ${DETECTORS})
    target_sources("sicm_${type}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${detector})
  endforeach()
endforeach()
