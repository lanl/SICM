set(SICM_PUBLIC_HEADERS
  sicm_low.h
  sicm.hpp
  sicm_arena_allocator.hpp
  sicm_device_allocator.hpp
)

install(FILES ${SICM_PUBLIC_HEADERS}
  DESTINATION include)
