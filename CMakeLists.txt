cmake_minimum_required(VERSION 2.8.3)
cmake_policy(SET CMP0042 NEW)

project(neko)

set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# put output in "bin"

set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# avoid the extra "Debug", "Release" directories
# http://stackoverflow.com/questions/7747857/in-cmake-how-do-i-work-around-the-debug-and-release-directories-visual-studio-2
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )


set(NEKO_VERSION_MAJOR 2)
set(NEKO_VERSION_MINOR 0)
set(NEKO_VERSION_PATCH 0)

include(CMakeDependentOption)
cmake_dependent_option(DIST "Build portable binaries for distribution. Implies STATIC_NDLL." OFF "NOT WIN32" ON)
cmake_dependent_option(STATIC_NDLL "Statically link dependencies to the ndll files." OFF "NOT WIN32;NOT DIST" ON)


if(UNIX AND NOT APPLE)
	set(CMAKE_SKIP_RPATH TRUE)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'\$ORIGIN',--enable-new-dtags")
	add_definitions(-DABI_ELF)
endif()

if(UNIX)
	add_definitions(-D_GNU_SOURCE)
	add_compile_options(-fno-omit-frame-pointer -fPIC)
endif()

if (WIN32)
	find_library(GC_STATIC_LIBRARIES NAMES gc.lib PATHS "./libs/include/gc" DOC "Boehm GC library")
	find_path(GC_INCLUDE_DIRS gc/gc.h "./libs/include" DOC "Boehm GC includes")
else()
	find_package(BoehmGC REQUIRED)
endif()

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
add_library(std.ndll MODULE ${std_SRC})
add_executable(nekovm ${vm_SRC})

target_include_directories(libneko PRIVATE ${GC_INCLUDE_DIRS})

if (STATIC_NDLL)
	target_link_libraries(libneko ${GC_STATIC_LIBRARIES})
else()
	target_link_libraries(libneko ${GC_LIBRARIES})
endif()
target_link_libraries(std.ndll libneko)
target_link_libraries(nekovm libneko)

if(WIN32)
	target_link_libraries(std.ndll ws2_32)
	add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gc.dll
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/libs/include/gc/gc.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)
	add_custom_target(gc.dll ALL
		DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gc.dll
	)
	add_dependencies(nekovm gc.dll)
endif()

if(MSVC)
	# seems to cause error in VS2010
	# find_library(MSVCRT_LIBRARIES NAMES extra.lib PATHS "./libs/include/msvcrt" DOC "msvcrt library")
	# target_link_libraries(libneko ${MSVCRT_LIBRARIES})
else()
	target_link_libraries(libneko dl m)
endif()

set_target_properties(nekovm libneko
	PROPERTIES
	OUTPUT_NAME neko
)
set_target_properties(libneko
	PROPERTIES
	VERSION 0.0.0
	SOVERSION 0
	COMPILE_DEFINITIONS "_USRDLL;NEKOVM_DLL_EXPORTS;NEKO_SOURCES"
)
set_target_properties(std.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME std
	SUFFIX .ndll
)

######################
# zlib.ndll

set(zlib.ndll_SRC
	libs/zlib/zlib.c
)

add_library(zlib.ndll MODULE ${zlib.ndll_SRC})

if (WIN32)
	find_library(ZLIB_STATIC_LIBRARIES NAMES zlib.lib PATHS "./libs/include/zlib" DOC "zlib library")
	find_path(ZLIB_INCLUDE_DIRS zlib.h "./libs/include/zlib" DOC "zlib includes")
else()
	find_package(Zlib REQUIRED)
endif()

target_include_directories(zlib.ndll PRIVATE ${ZLIB_INCLUDE_DIRS})

if (STATIC_NDLL)
	target_link_libraries(zlib.ndll libneko ${ZLIB_STATIC_LIBRARIES})
	if (UNIX AND NOT APPLE)
		get_target_property(zlib.ndll_COMPILE_OPTIONS zlib.ndll COMPILE_OPTIONS)
		list(REMOVE_ITEM zlib.ndll_COMPILE_OPTIONS -fPIC)
		set_target_properties(zlib.ndll
			PROPERTIES COMPILE_OPTIONS ${zlib.ndll_COMPILE_OPTIONS}
		)
	endif()
else()
	target_link_libraries(zlib.ndll libneko ${ZLIB_LIBRARIES})
endif()

set_target_properties(zlib.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME zlib
	SUFFIX .ndll
)

######################
# mysql.ndll

set(mysql.ndll_SRC
	libs/mysql/mysql.c
)

add_library(mysql.ndll MODULE ${mysql.ndll_SRC})

if (WIN32)
	find_library(MARIADB_CONNECTOR_STATIC_LIBRARIES NAMES mariadbclient.lib PATHS "./libs/include/mysql" DOC "MariaDB library")
	find_path(MARIADB_CONNECTOR_INCLUDE_DIR mysql.h "./libs/include/mysql" DOC "MariaDB includes")
	set(MARIADB_CONNECTOR_STATIC_FOUND FOUND)
	# target_link_libraries(mysql.ndll ws2_32)
else()
	find_package(MariaDBConnector REQUIRED)
	if (NOT APPLE)
		find_package(OpenSSL REQUIRED)
	endif()
endif()

target_include_directories(mysql.ndll
	PRIVATE
	${MARIADB_CONNECTOR_INCLUDE_DIR}
)

if(STATIC_NDLL)
	if(NOT MARIADB_CONNECTOR_STATIC_FOUND)
		message( FATAL_ERROR "Missing MariaDB static library." )
	endif()
	target_link_libraries(mysql.ndll libneko ${OPENSSL_LIBRARIES} ${MARIADB_CONNECTOR_STATIC_LIBRARIES})
else()
	target_link_libraries(mysql.ndll libneko ${OPENSSL_LIBRARIES} ${MARIADB_CONNECTOR_LIBRARIES})
endif()

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

add_library(mysql5.ndll MODULE ${mysql5.ndll_SRC})

target_include_directories(mysql5.ndll
	PRIVATE
	libs/mysql/my_proto
)

target_link_libraries(mysql5.ndll libneko)

if (WIN32)
	target_link_libraries(mysql5.ndll ws2_32)
endif()

set_target_properties(mysql5.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mysql5
	SUFFIX .ndll
)

######################
# regexp.ndll

set(regexp.ndll_SRC
	libs/regexp/regexp.c
)

if (WIN32)
	file(GLOB PCRE_SRC
		libs/include/pcre/*.c
	)
	list(APPEND regexp.ndll_SRC ${PCRE_SRC})
	find_path(PCRE_INCLUDE_DIRS pcre.h "./libs/include/pcre" DOC "PCRE includes")
else()
	if (STATIC_NDLL)
		set(PCRE_STATIC_LIBRARY TRUE)
	endif()
	find_package(PCRE REQUIRED)
endif()

add_library(regexp.ndll MODULE ${regexp.ndll_SRC})

target_include_directories(regexp.ndll PRIVATE ${PCRE_INCLUDE_DIRS})
target_link_libraries(regexp.ndll libneko ${PCRE_LIBRARIES})

set_target_properties(regexp.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME regexp
	SUFFIX .ndll
)

######################
# sqlite.ndll

if (WIN32)
	find_library(SQLITE3_STATIC_LIBRARIES NAMES sqlite3.lib PATHS "./libs/include/sqlite" DOC "SQLite library")
	find_path(SQLITE3_INCLUDE_DIRS sqlite3.h "./libs/include/sqlite" DOC "SQLite includes")
else()
	find_package(Sqlite3 REQUIRED)
endif()

set(sqlite.ndll_SRC
	libs/sqlite/sqlite.c
)

add_library(sqlite.ndll MODULE ${sqlite.ndll_SRC})

target_include_directories(sqlite.ndll PRIVATE ${SQLITE3_INCLUDE_DIRS})

if (STATIC_NDLL)
	target_link_libraries(sqlite.ndll libneko ${SQLITE3_STATIC_LIBRARIES})
else()
	target_link_libraries(sqlite.ndll libneko ${SQLITE3_LIBRARIES})
endif()

set_target_properties(sqlite.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME sqlite
	SUFFIX .ndll
)

######################
# ui.ndll

set(ui.ndll_SRC
	libs/ui/ui.c
)

add_library(ui.ndll MODULE ${ui.ndll_SRC})

target_link_libraries(ui.ndll libneko)

if(APPLE)
	find_library(CARBON_LIBRARY Carbon REQUIRED)
	target_link_libraries(ui.ndll ${CARBON_LIBRARY})
elseif(UNIX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
	target_include_directories(ui.ndll PRIVATE
		${GTK2_INCLUDE_DIRS}
	)
	target_link_libraries(ui.ndll ${GTK2_LIBRARIES})
endif()

set_target_properties(ui.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME ui
	SUFFIX .ndll
)

######################
# mod_neko2.ndll

if (WIN32)
	find_library(HTTPD_LIBRARY NAMES libhttpd
		PATHS "./libs/include/apache2" DOC "Apache library")
	find_library(APR_LIBRARY NAMES libapr-1.lib
		PATHS "./libs/include/apache2" DOC "Apache library")
	set(APACHE_LIBRARIES ${HTTPD_LIBRARY} ${APR_LIBRARY})
	find_path(APACHE_INCLUDE_DIRS httpd.h "./libs/include/apache2" DOC "Apache includes")
else()
	find_package(APACHE REQUIRED)
	find_package(APR REQUIRED)
	set(APACHE_LIBRARIES ${APR_LIBRARIES} ${APRUTIL_LIBRARIES})
	set(APACHE_INCLUDE_DIRS ${APACHE_INCLUDE_DIR} ${APR_INCLUDE_DIR} ${APRUTIL_INCLUDE_DIR})
endif()

set(mod_neko2.ndll_SRC
	vm/stats.c
	libs/mod_neko/mod_neko.c
	libs/mod_neko/cgi.c
)

add_library(mod_neko2.ndll MODULE ${mod_neko2.ndll_SRC})

target_include_directories(mod_neko2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIRS}
)

target_link_libraries(mod_neko2.ndll libneko ${APACHE_LIBRARIES})

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

add_library(mod_tora2.ndll MODULE ${mod_tora2.ndll_SRC})

target_include_directories(mod_tora2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIRS}
)

target_link_libraries(mod_tora2.ndll ${APACHE_LIBRARIES})
if (WIN32)
	target_link_libraries(mod_tora2.ndll ws2_32)
endif()

set_target_properties(mod_tora2.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mod_tora2
	SUFFIX .ndll
)

######################
# ssl.ndll

set(ssl.ndll_SRC
	libs/ssl/ssl.c
)

add_library(ssl.ndll MODULE ${ssl.ndll_SRC})

if (WIN32)
	find_library(MBEDTLS_STATIC_LIBRARIES NAMES mbedTLS.lib PATHS "./libs/include/ssl" DOC "mbedTLS library")
	find_path(MBEDTLS_INCLUDE_DIR mbedtls/ssl.h "./libs/include/ssl" DOC "mbedTLS includes")
	target_link_libraries(ssl.ndll ws2_32 Advapi32 Crypt32)
else()
	find_package(MbedTLS REQUIRED)
endif()

target_include_directories(ssl.ndll
	PRIVATE
	${MBEDTLS_INCLUDE_DIR}
)

if(APPLE)
	find_library(SECURITY_LIBRARY Security REQUIRED)
	find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
	target_link_libraries(ssl.ndll ${COREFOUNDATION_LIBRARY} ${SECURITY_LIBRARY})
endif()

if (STATIC_NDLL)
	target_link_libraries(ssl.ndll libneko ${MBEDTLS_STATIC_LIBRARIES})
else()
	target_link_libraries(ssl.ndll libneko ${MBEDTLS_LIBRARIES})
endif()

set_target_properties(ssl.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME ssl
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

if (WIN32)
	set(compilers_outputs
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.exe
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.exe
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.exe
	)
else()
	set(compilers_outputs
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools
	)
endif()

if (CMAKE_HOST_WIN32)
	set(set_neko_env set NEKOPATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
	set(neko_exec $<TARGET_FILE:nekovm>)
else()
	set(set_neko_env "")
	set(neko_exec NEKOPATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}:${LD_LIBRARY_PATH} $<TARGET_FILE:nekovm>)
endif()

add_custom_command(OUTPUT ${compilers_outputs} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test.n
	COMMAND ${set_neko_env}

	COMMAND ${neko_exec} ../boot/nekoml.n -nostd neko/Main.nml nekoml/Main.nml
	COMMAND ${neko_exec} ../boot/nekoc.n -link ../boot/nekoc.n neko/Main
	COMMAND ${neko_exec} ../boot/nekoc.n -link ../boot/nekoml.n nekoml/Main

	COMMAND ${CMAKE_COMMAND} -E copy ../boot/nekoc.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	COMMAND ${CMAKE_COMMAND} -E copy ../boot/nekoml.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}

	# compile some neko sources

	COMMAND ${neko_exec} ../boot/nekoc.n tools/test.neko
	COMMAND ${CMAKE_COMMAND} -E copy tools/test.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	COMMAND ${CMAKE_COMMAND} -E remove tools/test.n

	COMMAND ${neko_exec} ../boot/nekoc.n tools/nekoboot.neko
	COMMAND ${neko_exec} ../boot/nekoml.n -nostd -p tools Tools.nml
	COMMAND ${neko_exec} ../boot/nekoc.n -link tools/nekotools.n Tools
	COMMAND ${CMAKE_COMMAND} -E copy tools/nekotools.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}

	COMMAND ${neko_exec} tools/nekoboot ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n
	COMMAND ${neko_exec} tools/nekoboot ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
	COMMAND ${neko_exec} tools/nekoboot ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.n
	COMMAND ${CMAKE_COMMAND} -E remove
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.n

	VERBATIM
	DEPENDS nekovm std.ndll
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

file(GLOB CORE_NMLS RELATIVE ${CMAKE_SOURCE_DIR}/src src/core/*.nml)
set(nekoml_std ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.std)
add_custom_command(OUTPUT ${nekoml_std}
	COMMAND ${set_neko_env}
	COMMAND ${neko_exec} ../boot/nekoml.n -nostd neko/Main.nml nekoml/Main.nml ${CORE_NMLS} -pack ${nekoml_std}
	VERBATIM
	DEPENDS zlib.ndll ${compilers_outputs}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

add_custom_target(compilers ALL
	DEPENDS ${compilers_outputs} ${nekoml_std}
)

#######################

if (WIN32)
	set(source_archive_format zip)
else()
	set(source_archive_format tar.gz)
endif()

set(source_archive_name neko-${NEKO_VERSION_MAJOR}.${NEKO_VERSION_MINOR}.${NEKO_VERSION_PATCH}-src.${source_archive_format})

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name}
	COMMAND git archive -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name} HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(source_archive
	DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name}
)

#######################

# install target

if (WIN32) # implies DIST
	set(DEST_BIN .)
	set(DEST_LIB .)
	set(DEST_NDLL .)

	include(InstallRequiredSystemLibraries)
	install (
		FILES
			${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
			${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gc.dll
			${CMAKE_SOURCE_DIR}/README.md
			${CMAKE_SOURCE_DIR}/LICENSE
			${CMAKE_SOURCE_DIR}/CHANGES
		DESTINATION .
	)
else()
	if (DIST)
		set(DEST_BIN .)
		set(DEST_LIB .)
		set(DEST_NDLL .)
		install (
			FILES
				${CMAKE_SOURCE_DIR}/README.md
				${CMAKE_SOURCE_DIR}/LICENSE
				${CMAKE_SOURCE_DIR}/CHANGES
			DESTINATION .
		)
	else()
		set(DEST_BIN bin)
		set(DEST_LIB lib)
		set(DEST_NDLL lib/neko)
	endif()
endif()

if (UNIX AND NOT APPLE)
	set(CMAKE_INSTALL_PREFIX /usr)
endif()

install (
	TARGETS nekovm
	DESTINATION ${DEST_BIN}
)
install (
	PROGRAMS ${compilers_outputs}
	DESTINATION ${DEST_BIN}
)
install (
	FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.std
	DESTINATION ${DEST_BIN}
)
install (
	TARGETS libneko
	DESTINATION ${DEST_LIB}
)
install (
	TARGETS
		std.ndll
		zlib.ndll
		mysql.ndll
		mysql5.ndll
		regexp.ndll
		sqlite.ndll
		ui.ndll
		mod_neko2.ndll
		mod_tora2.ndll
		ssl.ndll
	DESTINATION ${DEST_NDLL}
)
install (
	DIRECTORY vm/
	DESTINATION include
	FILES_MATCHING PATTERN "neko*.h"
)

# uninstall target
configure_file(
	"${CMAKE_SOURCE_DIR}/cmake/uninstall.cmake.in"
	"${CMAKE_BINARY_DIR}/cmake/uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake/uninstall.cmake)

# package

set(CPACK_OUTPUT_FILE_PREFIX ${OUTPUT_DIR})

set(CPACK_PACKAGE_VERSION_MAJOR ${NEKO_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${NEKO_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${NEKO_VERSION_PATCH})

set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if (WIN32)
	set(CPACK_GENERATOR "ZIP")
	set(CPACK_SOURCE_GENERATOR "ZIP")
else()
	set(CPACK_GENERATOR "TGZ")
	set(CPACK_SOURCE_GENERATOR "TGZ")
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "i386")
		set(arch_64 "")
	elseif (${CMAKE_OSX_ARCHITECTURES} STREQUAL "x86_64")
		set(arch_64 "64")
	else()
		message( FATAL_ERROR "CMAKE_OSX_ARCHITECTURES should be i386 or x86_64." )
	endif()
else()
	if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
		set(arch_64 "64")
	else() 
		set(arch_64 "")
	endif() 
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(OS_NAME "win")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(OS_NAME "osx")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(OS_NAME "linux")
else()
	message( WARNING "unknown ${CMAKE_SYSTEM_NAME}" )
	set(OS_NAME "")
endif()

set(CPACK_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${OS_NAME}${arch_64}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-src")

include(CPack)

#######################

# tests

include(CTest)

add_test(NAME -version
	COMMAND nekovm -version
)

add_test(NAME test.n
	COMMAND nekovm test.n
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
