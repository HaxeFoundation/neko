# https://github.com/decimalbell/dust/blob/master/cmake/FindSqlite3.cmake

# FindSqlite3
# --------
#
# Find sqlite3
#
# Find the sqlite3 includes and library.  Once done this will define
#
#   SQLITE3_INCLUDE_DIRS   	 - where to find sqlite3.h, etc.
#   SQLITE3_LIBRARIES      	 - List of libraries when using sqlite3.
#   SQLITE3_STATIC_LIBRARIES      - List of static libraries when using sqlite3.
#   SQLITE3_FOUND          	 - True if sqlite3 found.
#

#=============================================================================
# Copyright 2001-2011 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h)
find_library(SQLITE3_LIBRARY  NAMES sqlite3)
find_library(SQLITE3_STATIC_LIBRARY  NAMES libsqlite3.a)

mark_as_advanced(SQLITE3_LIBRARY SQLITE3_STATIC_LIBRARY SQLITE3_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set SQLITE3_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Sqlite3 REQUIRED_VARS SQLITE3_LIBRARY SQLITE3_STATIC_LIBRARY SQLITE3_INCLUDE_DIR
                                          VERSION_VAR SQLITE3_VERSION_STRING)

if(SQLITE3_FOUND)
    set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
    set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
    set(SQLITE3_STATIC_LIBRARIES ${SQLITE3_STATIC_LIBRARY})
    message(STATUS "SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIRS}")
    message(STATUS "SQLITE3_LIBRARIES ${SQLITE3_LIBRARIES}")
    message(STATUS "SQLITE3_STATIC_LIBRARIES ${SQLITE3_STATIC_LIBRARIES}")
endif()
