set(ARENA_IMPLS
  mmap.c
)

foreach(type ${TYPES})
  foreach(arena_impl ${ARENA_IMPLS})
    target_sources("sicm_${type}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${arena_impl})
  endforeach()
endforeach()
