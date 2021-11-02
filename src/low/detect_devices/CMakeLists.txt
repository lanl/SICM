set(DETECTORS
  x86.c
  powerpc.c
  HIP.c
  DRAM.c
)

foreach(type ${TYPES})
  foreach(detector ${DETECTORS})
    target_sources("sicm_${type}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${detector})
  endforeach()
endforeach()
