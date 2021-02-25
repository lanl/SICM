add_library(nano OBJECT nano.c)

# example code
add_executable(move move.c)
target_link_libraries(move PUBLIC sicm_SHARED)
target_link_libraries(move PRIVATE "${JEMALLOC_LDFLAGS}")

# move individual pointers using loop
add_executable(loop_move_perf loop_move_perf.c nano)
target_link_libraries(loop_move_perf PUBLIC sicm_SHARED)
target_link_libraries(loop_move_perf PRIVATE "${JEMALLOC_LDFLAGS}")

# move all pointers at once
add_executable(bulk_move_perf bulk_move_perf.c nano)
target_link_libraries(bulk_move_perf PUBLIC sicm_SHARED)
target_link_libraries(bulk_move_perf PRIVATE "${JEMALLOC_LDFLAGS}")

# C++ arena allocator
add_executable(arena_allocator_example arena_allocator.cpp)
set_target_properties(arena_allocator_example PROPERTIES OUTPUT_NAME "arena_allocator")
target_link_libraries(arena_allocator_example PUBLIC sicm_SHARED)
target_link_libraries(arena_allocator_example PRIVATE "${JEMALLOC_LDFLAGS}")

# C++ device allocator
add_executable(device_allocator_example device_allocator.cpp)
set_target_properties(device_allocator_example PROPERTIES OUTPUT_NAME "device_allocator")
target_link_libraries(device_allocator_example PUBLIC sicm_SHARED)
target_link_libraries(device_allocator_example PRIVATE "${JEMALLOC_LDFLAGS}")
