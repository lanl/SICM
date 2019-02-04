add_library(sicm_high SHARED sicm_high.c sicm_profile.c)

# Public and private headers for each library
target_include_directories(sicm_high PRIVATE ../../include/private)
target_include_directories(sicm_high PUBLIC ../../include/public)

find_package(Jemalloc REQUIRED)
target_include_directories(sicm_high PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_high ${JEMALLOC_LIBRARIES})

find_package(NUMA REQUIRED)
target_include_directories(sicm_high PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_high ${NUMA_LIBRARY})

find_package(LibPfm4 REQUIRED)
target_include_directories(sicm_high PRIVATE ${LIBPFM_INCLUDE_DIR})
target_link_libraries(sicm_high ${LIBPFM_LIBRARY})

install(TARGETS sicm_high
        LIBRARY DESTINATION lib)
