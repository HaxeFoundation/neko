# https://github.com/CBL-ORION/Vaa3D-tools-mirror/blob/filter-large-files/released_plugins/v3d_plugins/neurontracing_neutube/src_neutube/neurolabi/c/cmake_modules/FindPCRE.cmake

# - Try to find PCRE
# Once done, this will define
#
#  PCRE_FOUND - system has PCRE
#  PCRE_INCLUDE_DIRS - the PCRE include directories
#  PCRE_LIBRARIES - link these to use PCRE
#
# By default, the dynamic libraries will be found. To find the static ones instead,
# you must set the PCRE_STATIC_LIBRARY variable to TRUE before calling find_package.
#

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(PCRE_PKGCONF libpcre)

# attempt to find static library first if this is set
if(PCRE_STATIC_LIBRARY)
  set(PCRE_POSIX_STATIC libpcreposix.a)
  set(PCRE_STATIC libpcre.a)
endif(PCRE_STATIC_LIBRARY)

# additional hints
if(MINGW)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} C:/Mingw)
endif(MINGW)

if(MSVC)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} D:/devlib/vc/PCRE)
endif(MSVC)

# Include dir
find_path(PCRE_INCLUDE_DIR
  NAMES pcreposix.h
  PATHS ${PCRE_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(PCRE_POSIX_LIBRARY
  NAMES ${PCRE_POSIX_STATIC} pcreposix
  PATHS ${PCRE_PKGCONF_LIBRARY_DIRS}
)
find_library(PCRE_LIBRARY
  NAMES ${PCRE_STATIC} pcre
  PATHS ${PCRE_PKGCONF_LIBRARY_DIRS}
)
set(PCRE_POSIX_LIBRARY ${PCRE_POSIX_LIBRARY} ${PCRE_LIBRARY})

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(PCRE_PROCESS_INCLUDES PCRE_INCLUDE_DIR)
set(PCRE_PROCESS_LIBS PCRE_POSIX_LIBRARY)
libfind_process(PCRE)