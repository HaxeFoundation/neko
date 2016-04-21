# ZLIB_ROOT hints the location
# Provides
# - ZLIB,
# - ZLIB_LIBRARIES,
# - ZLIB_STATIC_LIBRARIES,
# - ZLIB_FOUND

find_path(ZLIB_INCLUDE_DIR zlib.h
  PATH_SUFFIXES include)

find_library(ZLIB_STATIC_LIBRARIES libz.a
  PATH_SUFFIXES lib lib64)

set(ZLIB_NAMES z zlib zdll zlib1 zlibd zlibd1)
find_library(ZLIB_LIBRARIES ${ZLIB_NAMES}
  PATH_SUFFIXES lib lib64)

if (NOT ZLIB_LIBRARIES AND NOT ZLIB_STATIC_LIBRARIES)
  message(FATAL_ERROR "zlib not found in ${ZLIB_ROOT}")
  set(ZLIB_FOUND FALSE)
else()
  message(STATUS "Zlib: ${ZLIB_INCLUDE_DIR}")
  message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
  message(STATUS "ZLIB_STATIC_LIBRARIES: ${ZLIB_STATIC_LIBRARIES}")
  set(ZLIB_FOUND TRUE)
endif()

mark_as_advanced(
  ZLIB_INCLUDE_DIR
  ZLIB_LIBRARIES
  ZLIB_STATIC
  ZLIB_STATIC_FOUND
)