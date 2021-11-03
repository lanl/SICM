add_library(nano OBJECT nano.c)
add_library(sizes OBJECT sizes.c)

# simple executable for generating sizes
add_executable(generate_sizes generate_sizes.c sizes)

# example code
add_executable(move move.c sizes)
target_link_libraries(move PUBLIC sicm_SHARED)
target_link_libraries(move PRIVATE ${JEMALLOC_LDFLAGS})

# move individual pointers using loop
add_executable(loop_move_perf loop_move_perf.c nano sizes)
target_link_libraries(loop_move_perf PUBLIC sicm_SHARED)
target_link_libraries(loop_move_perf PRIVATE ${JEMALLOC_LDFLAGS})

# move all pointers at once
add_executable(bulk_move_perf bulk_move_perf.c nano sizes)
target_link_libraries(bulk_move_perf PUBLIC sicm_SHARED)
target_link_libraries(bulk_move_perf PRIVATE ${JEMALLOC_LDFLAGS})

# simple plotting script for loop_move_perf and bulk_move_perf
configure_file(plot_moves.sh plot_moves.sh @ONLY)

# C++ allocators
add_executable(allocators allocators.cpp)
target_link_libraries(allocators PUBLIC sicm_SHARED)
target_link_libraries(allocators PRIVATE ${JEMALLOC_LDFLAGS})
