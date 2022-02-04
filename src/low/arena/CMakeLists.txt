set(ARENA_IMPLS
  mmap.c
)

if (hip_FOUND)
  list(APPEND ARENA_IMPLS HIP.c)
endif()

foreach(type ${TYPES})
  foreach(arena_impl ${ARENA_IMPLS})
    target_sources("sicm_${type}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${arena_impl})
  endforeach()
endforeach()
