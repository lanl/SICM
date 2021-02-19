add_library(nano OBJECT nano.c)

add_executable(move_perf move_perf.c)
target_link_libraries(move_perf PUBLIC sicm_SHARED nano)
target_link_libraries(move_perf PRIVATE "${JEMALLOC_LDFLAGS}")

add_executable(bulk_move_perf bulk_move_perf.c)
target_link_libraries(bulk_move_perf PUBLIC sicm_SHARED nano)
target_link_libraries(bulk_move_perf PRIVATE "${JEMALLOC_LDFLAGS}")
