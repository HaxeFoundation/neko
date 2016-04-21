# file(DOWNLOAD ...) at build time
# usage: ${CMAKE_COMMAND} -Durl=url -Dfile=path/to/output_file -P ${CMAKE_SOURCE_DIR}/cmake/download.cmake
# https://cmake.org/pipermail/cmake/2013-March/053915.html
file(DOWNLOAD ${url} ${file})
