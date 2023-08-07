#build snap example

add_executable(snap control.c data.c dealloc.c dim1_sweep.c dim3_sweep.c expxs.c geom.c inner.c input.c mms.c octsweep.c outer.c output.c plib.c setup.c snap_main.c sn.c solvar.c sweep.c time.c translv.c utils.c version.c)

target_link_libraries(snap PUBLIC sicm_SHARED)
target_link_libraries(snap PRIVATE ${JEMALLOC_LDFLAGS})
configure_file(center_src/fin01 ${CMAKE_CURRENT_BINARY_DIR}/fin01 COPYONLY)

