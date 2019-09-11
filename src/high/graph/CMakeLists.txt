add_library(sicm_graph SHARED sicm_graph.c)
add_library(sicm_layout SHARED sicm_layout.c)

# Public and private headers for each library
target_include_directories(sicm_graph PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_graph PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)

target_include_directories(sicm_layout PRIVATE ${CMAKE_SOURCE_DIR}/include/high/private)
target_include_directories(sicm_layout PUBLIC ${CMAKE_SOURCE_DIR}/include/high/public)

target_link_libraries(sicm_graph  sicm)
target_link_libraries(sicm_layout sicm)

find_package(Threads)

####################
#     libnuma      #
####################
find_package(NUMA REQUIRED)
target_include_directories(sicm_graph PRIVATE ${NUMA_INCLUDE_DIR})
target_link_libraries(sicm_graph ${NUMA_LIBRARY})

####################
#        rt        #
####################
find_library(LIBRT rt)
target_link_libraries(sicm_graph ${LIBRT})

install(TARGETS sicm_graph
        LIBRARY DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/high/public/
        DESTINATION include/)
