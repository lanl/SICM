add_library(sicm_runtime SHARED 
            sicm_runtime.c
            sicm_runtime_init.c
            sicm_rdspy.c
            sicm_profile.c
            profilers/sicm_profile_all.c
            profilers/sicm_profile_allocs.c
            profilers/sicm_profile_rss.c
            profilers/sicm_profile_extent_size.c
            profilers/sicm_profile_online.c
            profilers/sicm_profile_one.c)
add_library(sicm_overrides SHARED 
            sicm_cstd.c)
add_library(sicm_compass SHARED sicm_compass.cpp)
add_library(sicm_rdspy SHARED sicm_rdspy.cpp)
add_executable(sicm_memreserve sicm_memreserve.c)
add_executable(sicm_hotset sicm_hotset.c)

# Public and private headers for each library
target_include_directories(sicm_runtime PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_runtime PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_runtime PRIVATE ${CMAKE_SOURCE_DIR}/include/low/private)
target_include_directories(sicm_runtime PUBLIC ${CMAKE_SOURCE_DIR}/include/low/public)
target_include_directories(sicm_overrides PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_overrides PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_memreserve PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)
target_include_directories(sicm_hotset PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)

target_link_libraries(sicm_runtime sicm)

find_package(Threads)

####################
#     jemalloc     #
####################
find_package(Jemalloc REQUIRED)
target_include_directories(sicm_runtime PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_runtime ${JEMALLOC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(sicm_overrides PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_overrides ${JEMALLOC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(sicm_memreserve PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_memreserve ${JEMALLOC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(sicm_hotset PRIVATE ${JEMALLOC_INCLUDE_DIR})
target_link_libraries(sicm_hotset ${JEMALLOC_LIBRARIES})

####################
#     libnuma      #
####################
find_package(NUMA REQUIRED)
target_include_directories(sicm_runtime PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_runtime ${NUMA_LIBRARY})
target_include_directories(sicm_memreserve PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_memreserve ${NUMA_LIBRARY})
target_include_directories(sicm_hotset PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_hotset ${NUMA_LIBRARY})

####################
#     libpfm4      #
####################
find_package(LibPfm4 REQUIRED)
target_include_directories(sicm_runtime PRIVATE ${LIBPFM_INCLUDE_DIR})
target_link_libraries(sicm_runtime ${LIBPFM_LIBRARY})

####################
#       LLVM       #
####################
find_package(LLVM REQUIRED CONFIG)
target_include_directories(sicm_compass PRIVATE ${LLVM_INCLUDE_DIR})
target_link_libraries(sicm_compass ${LLVM_LIBRARIES})
target_include_directories(sicm_rdspy PRIVATE ${LLVM_INCLUDE_DIR})
target_link_libraries(sicm_rdspy ${LLVM_LIBRARIES})

####################
#        rt        #
####################
find_library(LIBRT rt)
target_link_libraries(sicm_runtime ${LIBRT})

install(TARGETS sicm_runtime sicm_overrides sicm_compass sicm_rdspy sicm_memreserve sicm_hotset
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/high/public/
        DESTINATION include/)
