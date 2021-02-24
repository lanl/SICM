set(SICM_PUBLIC_HEADERS
  sicm_low.h
  sicm.hpp
  sicm-arena-03.hpp
  sicm-arena-11.hpp
  sicm-device-03.hpp
  sicm-device-11.hpp
)

install(FILES ${SICM_PUBLIC_HEADERS}
  DESTINATION include)
