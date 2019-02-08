add_library(sicm SHARED sicm_low.c sicm_arena.c
            ${CMAKE_SOURCE_DIR}/include/low/public/sicm_low.h
            ${CMAKE_SOURCE_DIR}/include/low/private/sicm_impl.h) # Header files
add_library(sicm_f90 SHARED fbinding_c.c fbinding_f90.f90)
add_library(sicm_cpp SHARED sicm_cpp.cpp
            ${CMAKE_SOURCE_DIR}/include/low/public/sicm_cpp.hpp) # Header files

target_include_directories(sicm PRIVATE ${CMAKE_SOURCE_DIR}/low/include/private)
target_include_directories(sicm PUBLIC ${CMAKE_SOURCE_DIR}/low/include/public)
target_include_directories(sicm_f90 PRIVATE ${CMAKE_SOURCE_DIR}/low/include/private)
target_include_directories(sicm_f90 PUBLIC ${CMAKE_SOURCE_DIR}/low/include/public)
target_include_directories(sicm_cpp PRIVATE ${CMAKE_SOURCE_DIR}/low/include/private)
target_include_directories(sicm_cpp PUBLIC ${CMAKE_SOURCE_DIR}/low/include/public)

find_package(Jemalloc REQUIRED)
target_link_libraries(sicm ${JEMALLOC_LIBRARIES})
target_include_directories(sicm PRIVATE ${JEMALLOC_INCLUDE_DIR})

find_package(NUMA REQUIRED)
target_link_libraries(sicm ${NUMA_LIBRARY})
target_include_directories(sicm PRIVATE ${NUMA_INCLUDE_DIR})

install(TARGETS sicm sicm_f90 sicm_cpp
        LIBRARY DESTINATION lib
        INCLUDES DESTINATION include)
