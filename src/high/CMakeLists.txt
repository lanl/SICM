add_library(sicm_high SHARED sicm_high.c sicm_profile.c sicm_rdspy.c)
add_library(sicm_compass SHARED sicm_compass.cpp)
add_library(sicm_rdspy SHARED sicm_rdspy.cpp)
add_executable(sicm_dump_info sicm_dump_info.c)
add_executable(sicm_memreserve sicm_memreserve.c)
add_executable(sicm_hotset sicm_hotset.c)

# Public and private headers for each library
target_include_directories(sicm_high PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_high PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_high PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm_high PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)
target_include_directories(sicm_dump_info PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_dump_info PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_memreserve PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_memreserve PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_memreserve PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm_memreserve PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)
target_include_directories(sicm_hotset PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_hotset PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_hotset PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm_hotset PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)

####################
#     jemalloc     #
####################
find_package(Jemalloc REQUIRED)
target_include_directories(sicm_high PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_high ${JEMALLOC_LIBRARIES})
target_include_directories(sicm_dump_info PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_dump_info ${JEMALLOC_LIBRARIES})
target_include_directories(sicm_memreserve PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_memreserve ${JEMALLOC_LIBRARIES})
target_include_directories(sicm_hotset PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_hotset ${JEMALLOC_LIBRARIES})

####################
#     libnuma      #
####################
find_package(NUMA REQUIRED)
target_include_directories(sicm_high PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_high ${NUMA_LIBRARY})
target_include_directories(sicm_dump_info PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_dump_info ${NUMA_LIBRARY})
target_include_directories(sicm_memreserve PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_memreserve ${NUMA_LIBRARY})
target_include_directories(sicm_hotset PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_hotset ${NUMA_LIBRARY})

####################
#     libpfm4      #
####################
find_package(LibPfm4 REQUIRED)
target_include_directories(sicm_high PRIVATE ${LIBPFM_INCLUDE_DIR})
target_link_libraries(sicm_high ${LIBPFM_LIBRARY})

####################
#       LLVM       #
####################
find_package(LLVM REQUIRED CONFIG)
target_include_directories(sicm_compass PRIVATE ${LLVM_INCLUDE_DIR})
target_link_libraries(sicm_compass ${LLVM_LIBRARIES})
target_include_directories(sicm_rdspy PRIVATE ${LLVM_INCLUDE_DIR})
target_link_libraries(sicm_rdspy ${LLVM_LIBRARIES})

####################
#     PThreads     #
####################
target_link_libraries(sicm_memreserve pthread)

install(TARGETS sicm_high sicm_compass sicm_rdspy sicm_dump_info sicm_memreserve sicm_hotset
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
