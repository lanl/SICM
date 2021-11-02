find_package(NUMA REQUIRED)

list(APPEND CMAKE_PREFIX_PATH /opt/rocm/hip /opt/rocm)
find_package(hip)

# whether or not to build the static library
# the shared libraries are always built
option(SICM_STATIC_LIB "Build the static library" Off)
list(APPEND TYPES "SHARED")
if (SICM_STATIC_LIB)
  list(APPEND TYPES "STATIC")
endif()

# create_library(target_name <SHARED|STATIC> [sources])
function(create_library name type)
  set(target "${name}_${type}")
  add_library(${target} ${type} ${ARGN})
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
  target_include_directories(${target} PUBLIC  ${CMAKE_SOURCE_DIR}/include/low/public)
  target_include_directories(${target} PRIVATE ${JEMALLOC_INCLUDE_DIRS})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${name}")
  install(TARGETS ${target}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
endfunction()

# build source files for the shared and static libraries separately to not incur PIC penalties
foreach(type ${TYPES})
  create_library(sicm ${type} sicm_low.c sicm_arena.c detect_devices.c
    ${SICM_SOURCE_DIR}/include/low/public/sicm_low.h)
  create_library(sicm_f90 ${type} fbinding_c.c fbinding_f90.f90)

  set(target "sicm_${type}")

  # libsicm needs to link against jemalloc and numa
  target_link_libraries(${target} ${JEMALLOC_LDFLAGS})
  target_link_libraries(${target} ${NUMA_LIBRARY})
  if (hip_FOUND)
    target_compile_definitions(${target} PUBLIC HIP=1)
    target_link_libraries(${target} hip::host)
  endif()
endforeach()

add_subdirectory(detect_devices)
