add_library(sicm SHARED sicm_low.c sicm_arena.c sicm_low.h sicm_impl.h)
add_library(sicm_f90 SHARED fbinding_c.c fbinding_f90.f90 sicm_low.h sicm_impl.h)
add_library(sicm_cpp SHARED sicm_cpp.cpp sicm_cpp.hpp)

target_include_directories(sicm PRIVATE ../../include/private)
target_include_directories(sicm PUBLIC ../../include/public)
target_include_directories(sicm_f90 PRIVATE ../../include/private)
target_include_directories(sicm_f90 PUBLIC ../../include/public)
target_include_directories(sicm_cpp PRIVATE ../../include/private)
target_include_directories(sicm_cpp PUBLIC ../../include/public)

find_package(Jemalloc REQUIRED)
target_link_libraries(sicm ${JEMALLOC_LIBRARIES})
target_include_directories(sicm PRIVATE ${JEMALLOC_INCLUDE_DIR})

find_package(NUMA REQUIRED)
target_link_libraries(sicm ${NUMA_LIBRARY})
target_include_directories(sicm PRIVATE ${NUMA_INCLUDE_DIR})

install(TARGETS sicm sicm_f90 sicm_cpp
        LIBRARY DESTINATION lib
        INCLUDES DESTINATION include)
