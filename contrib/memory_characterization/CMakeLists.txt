cmake_minimum_required (VERSION 3.0)

# check if the kmeans directory has been intialized
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kmeans/.git")
  find_package(OpenMP)
  if (OPENMP_FOUND)
    add_executable(memory_characterization memory_characterization.c "${CMAKE_CURRENT_SOURCE_DIR}/kmeans/kmeans.c")
    target_include_directories(memory_characterization PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(memory_characterization ${OpenMP_C_FLAGS} numa)
    configure_file(memory_characterization.sh memory_characterization.sh COPYONLY)
  endif()
endif()
