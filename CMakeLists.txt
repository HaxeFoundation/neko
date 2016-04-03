cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0042 NEW)

# TODO: support out-of-source builds by modifying the "compilers" target
if ( NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
    message( FATAL_ERROR "Out-of-source builds not allowed. Please run CMake from the project root." )
endif()

set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(NEKO_VERSION_MAJOR 2)
set(NEKO_VERSION_MINOR 0)
set(NEKO_VERSION_PATCH 0)

project(neko)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GC REQUIRED bdw-gc)

include_directories(
	vm
	libs/common
)

set(vm_SRC
	vm/stats.c
	vm/main.c
)

set(std_SRC
	libs/std/buffer.c
	libs/std/date.c
	libs/std/file.c
	libs/std/init.c
	libs/std/int32.c
	libs/std/math.c
	libs/std/string.c
	libs/std/random.c
	libs/std/serialize.c
	libs/std/socket.c
	libs/std/sys.c
	libs/std/xml.c
	libs/std/module.c
	libs/common/sha1.c
	libs/std/md5.c
	libs/std/unicode.c
	libs/std/utf8.c
	libs/std/memory.c
	libs/std/misc.c
	libs/std/thread.c
	libs/std/process.c
	libs/std/elf_update.c
)

set(lib_SRC
    vm/alloc.c
    vm/builtins.c
    vm/callback.c
    vm/elf.c
    vm/interp.c
    vm/load.c
    vm/objtable.c
    vm/others.c
    vm/hash.c
    vm/module.c
    vm/jit_x86.c
    vm/threads.c
)

add_library(libneko SHARED ${lib_SRC})
add_library(std.ndll SHARED ${std_SRC})
add_executable(nekovm ${vm_SRC})

target_link_libraries(libneko ${GC_LIBRARIES})
target_link_libraries(std.ndll libneko)
target_link_libraries(nekovm libneko)

set_target_properties(nekovm libneko
	PROPERTIES
	OUTPUT_NAME neko
)
set_target_properties(libneko
	PROPERTIES
	VERSION 0.0.0
	SOVERSION 0
)
set_target_properties(std.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME std
	SUFFIX .ndll
)

######################
# zlib.ndll

find_package(ZLIB REQUIRED)
if(NOT ZLIB_FOUND)
	pkg_check_modules(ZLIB REQUIRED zlib)
endif(NOT ZLIB_FOUND)

set(zlib.ndll_SRC
	libs/zlib/zlib.c
)

add_library(zlib.ndll SHARED ${zlib.ndll_SRC})

target_link_libraries(zlib.ndll libneko ${ZLIB_LIBRARIES})

set_target_properties(zlib.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME zlib
	SUFFIX .ndll
)

######################
# mysql.ndll

pkg_check_modules(MYSQL REQUIRED mariadb)
if(NOT MYSQL_FOUND)
	pkg_check_modules(MYSQL REQUIRED mysqlclient)
endif(NOT MYSQL_FOUND)

add_library(mysql_static STATIC IMPORTED)
set_target_properties(mysql_static
	PROPERTIES
	IMPORTED_LOCATION ${MYSQL_LIBRARY_DIRS}/libmysqlclient_r.a
)

find_package(OPENSSL REQUIRED)
if(NOT OPENSSL_FOUND)
	pkg_check_modules(OPENSSL REQUIRED openssl)
endif(NOT OPENSSL_FOUND)

set(mysql.ndll_SRC
	libs/mysql/mysql.c
)

add_library(mysql.ndll SHARED ${mysql.ndll_SRC})

target_include_directories(mysql.ndll
	PRIVATE
	${MYSQL_INCLUDE_DIRS}
)

target_link_libraries(mysql.ndll libneko mysql_static ${OPENSSL_LIBRARIES})

set_target_properties(mysql.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mysql
	SUFFIX .ndll
)

######################
# mysql5.ndll

set(mysql5.ndll_SRC
	libs/common/sha1.c
	libs/common/socket.c
	libs/mysql/my_proto/my_proto.c
	libs/mysql/my_proto/my_api.c
	libs/mysql/mysql.c
)

add_library(mysql5.ndll SHARED ${mysql5.ndll_SRC})

target_include_directories(mysql5.ndll
	PRIVATE
	libs/mysql/my_proto
)

target_link_libraries(mysql5.ndll libneko)

set_target_properties(mysql5.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mysql5
	SUFFIX .ndll
)

######################
# regexp.ndll

pkg_check_modules(PCRE REQUIRED libpcre)

set(regexp.ndll_SRC
	libs/regexp/regexp.c
)

add_library(regexp.ndll SHARED ${regexp.ndll_SRC})

target_link_libraries(regexp.ndll libneko ${PCRE_LIBRARIES})

set_target_properties(regexp.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME regexp
	SUFFIX .ndll
)

######################
# sqlite.ndll

pkg_check_modules(SQLITE REQUIRED sqlite3)

set(sqlite.ndll_SRC
	libs/sqlite/sqlite.c
)

add_library(sqlite.ndll SHARED ${sqlite.ndll_SRC})

target_link_libraries(sqlite.ndll libneko ${SQLITE_LIBRARIES})

set_target_properties(sqlite.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME sqlite
	SUFFIX .ndll
)

######################
# ui.ndll

if(APPLE)
   find_library(CARBON_LIBRARY Carbon REQUIRED)
endif(APPLE)

set(ui.ndll_SRC
	libs/ui/ui.c
)

add_library(ui.ndll SHARED ${ui.ndll_SRC})

target_link_libraries(ui.ndll libneko ${CARBON_LIBRARY})

set_target_properties(ui.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME ui
	SUFFIX .ndll
)

######################
# mod_neko2.ndll

find_package(APACHE REQUIRED)
find_package(APR REQUIRED)

set(mod_neko2.ndll_SRC
	vm/stats.c
	libs/mod_neko/mod_neko.c
	libs/mod_neko/cgi.c
)

add_library(mod_neko2.ndll SHARED ${mod_neko2.ndll_SRC})

target_include_directories(mod_neko2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIR}
	${APR_INCLUDE_DIR}
	${APRUTIL_INCLUDE_DIR}
)

target_link_libraries(mod_neko2.ndll libneko ${APR_LIBRARIES} ${APRUTIL_LIBRARIES})

set_target_properties(mod_neko2.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mod_neko2
	SUFFIX .ndll
)

if(APPLE)
	set_target_properties(mod_neko2.ndll
		PROPERTIES
		LINK_FLAGS "-undefined dynamic_lookup ${LINK_FLAGS}"
	)
endif(APPLE)

######################
# mod_tora2.ndll

set(mod_tora2.ndll_SRC
	libs/common/socket.c
	libs/mod_tora/protocol.c
	libs/mod_tora/mod_tora.c
)

add_library(mod_tora2.ndll SHARED ${mod_tora2.ndll_SRC})

target_include_directories(mod_tora2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIR}
	${APR_INCLUDE_DIR}
	${APRUTIL_INCLUDE_DIR}
)

target_link_libraries(mod_tora2.ndll ${APR_LIBRARIES} ${APRUTIL_LIBRARIES})

set_target_properties(mod_tora2.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mod_tora2
	SUFFIX .ndll
)


#######################

if(APPLE)
	set_target_properties(mod_tora2.ndll mysql.ndll
		PROPERTIES
		LINK_FLAGS "-undefined dynamic_lookup ${LINK_FLAGS}"
	)
endif(APPLE)


#######################

get_target_property(NEKOVM_NAME nekovm OUTPUT_NAME)
set(NEKO_EXEC LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}:${LD_LIBRARY_PATH} NEKOPATH=${CMAKE_SOURCE_DIR}/boot:${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${NEKOVM_NAME})

add_custom_target(compilers ALL
	COMMAND ${NEKO_EXEC} nekoml -nostd neko/Main.nml nekoml/Main.nml
	COMMAND ${NEKO_EXEC} nekoc -link ../boot/nekoc.n neko/Main
	COMMAND ${NEKO_EXEC} nekoc -link ../boot/nekoml.n nekoml/Main
	COMMAND ${NEKO_EXEC} nekoc tools/install.neko
	COMMAND ${NEKO_EXEC} tools/install
	DEPENDS nekovm std.ndll zlib.ndll
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

#######################

add_custom_target(package
	COMMAND git archive -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/neko-${NEKO_VERSION_MAJOR}.${NEKO_VERSION_MINOR}.${NEKO_VERSION_PATCH}.tar.gz HEAD
)
