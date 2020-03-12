# https://github.com/bro/cmake/blob/master/FindJeMalloc.cmake
#
# - Try to find jemalloc headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(JeMalloc)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  JEMALLOC_ROOT Set this variable to the root installation of
#                    jemalloc if the module has problems finding
#                    the proper installation path.
#
# Variables defined by this module:
#
#  JEMALLOC_FOUND             System has jemalloc libs/headers
#  JEMALLOC_LIBRARIES         The jemalloc library/libraries
#  JEMALLOC_INCLUDE_DIR       The location of jemalloc headers

find_path(JEMALLOC_ROOT
    NAMES include/jemalloc/jemalloc.h
)

find_library(JEMALLOC_LIBRARIES
    NAMES jemalloc
    HINTS ${JEMALLOC_ROOT}/lib
)

find_path(JEMALLOC_INCLUDE_DIR
    NAMES jemalloc/jemalloc.h
    HINTS ${JEMALLOC_ROOT}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JeMalloc DEFAULT_MSG
    JEMALLOC_LIBRARIES
    JEMALLOC_INCLUDE_DIR
)

mark_as_advanced(
    JEMALLOC_ROOT
    JEMALLOC_LIBRARIES
    JEMALLOC_INCLUDE_DIR
)
