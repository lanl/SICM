add_library(sicm SHARED sicm_low.c sicm_arena.c
            ${SICM_SOURCE_DIR}/include/low/public/sicm_low.h)
add_library(sicm_f90 SHARED fbinding_c.c fbinding_f90.f90)
add_library(sicm_cpp SHARED sicm_cpp.cpp
            ${CMAKE_SOURCE_DIR}/include/low/public/sicm_cpp.hpp)

target_include_directories(sicm PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)
target_include_directories(sicm_f90 PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm_f90 PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)
target_include_directories(sicm_cpp PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm_cpp PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)

find_package(Jemalloc REQUIRED)
target_link_libraries(sicm ${JEMALLOC_LIBRARIES})
target_include_directories(sicm PRIVATE ${JEMALLOC_INCLUDE_DIR})

find_package(NUMA REQUIRED)
target_link_libraries(sicm ${NUMA_LIBRARY})
target_include_directories(sicm PRIVATE ${NUMA_INCLUDE_DIR})

target_link_libraries(sicm pthread)

install(TARGETS sicm sicm_f90 sicm_cpp
        LIBRARY DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/low/public/
        DESTINATION include/)
