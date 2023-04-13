# This is necessary so that the CMAKE_FIND_LIBRARY_SUFFIXES is preserved when
# locating expat, since it gets reset when PROJECT is called

set(cmakelists "${apr-util_source}/CMakeLists.txt")

file(READ ${cmakelists} content)

string(REPLACE
"PROJECT(APR-Util C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

FIND_PACKAGE(OpenSSL)

FIND_PACKAGE(EXPAT)"

"CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

FIND_PACKAGE(OpenSSL)

FIND_PACKAGE(EXPAT)

PROJECT(APR-Util C)"

	content "${content}"
)

file(WRITE ${cmakelists} "${content}")
