add_library(sicm SHARED sicm_low.c sicm_arena.c)

target_include_directories(sicm PRIVATE ../../include/private)
target_include_directories(sicm PUBLIC ../../include/public)

find_package(Jemalloc REQUIRED)
target_link_libraries(sicm ${JEMALLOC_LIBRARIES})
target_include_directories(sicm ${JEMALLOC_INCLUDE_DIR})

find_package(NUMA REQUIRED)
target_link_libraries(sicm ${NUMA_LIBRARY})
target_include_directories(sicm ${NUMA_INCLUDE_DIR})
