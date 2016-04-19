cmake_minimum_required(VERSION 2.8.7)

project(neko C)

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

# target_include_directories for old cmake versions
if (${CMAKE_VERSION} VERSION_LESS 2.8.8)
	function(target_include_directories target visibility)
		include_directories(${ARGN})
	endfunction()
elseif (${CMAKE_VERSION} VERSION_LESS 2.8.11)
	function(target_include_directories target visibility)
		set_target_properties(${target}
			PROPERTIES
			INCLUDE_DIRECTORIES "${INCLUDE_DIRECTORIES} ${ARGN}"
		)
	endfunction()
endif()

# add_compile_options for old cmake versions
if (${CMAKE_VERSION} VERSION_LESS 2.8.12)
	macro(add_compile_options)
		foreach(opt ${ARGN})
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${opt}")
		endforeach()
	endmacro()
endif()

set(NEKO_VERSION_MAJOR 2)
set(NEKO_VERSION_MINOR 0)
set(NEKO_VERSION_PATCH 0)

set(ndll_targets
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
)

set(external_deps
	BoehmGC
	Zlib
	OpenSSL
	MariaDBConnector
	PCRE
	Sqlite3
	APR
	APRutil
	Apache
	MbedTLS
)

set(WITH_NDLLS ${ndll_targets} CACHE STRING "A list of ndll files to be built.")

if (WIN32)
	set(STATIC_DEPS "all")
else()
	set(STATIC_DEPS "none" CACHE STRING "Dependencies that should be linked statically. Can be \"all\", \"none\", or a list of library names (e.g. \"${external_deps}\").")

	option(RELOCATABLE "Set RPATH to $ORIGIN (Linux) / @executable_path (Mac)." ON)

	if (NOT APPLE)
		option(RUN_LDCONFIG "Run ldconfig after install." ON)
	endif()
endif()


# These ndll cannot be skipped.
set(core_ndll_targets
	std.ndll
	zlib.ndll
)

# Validate STATIC_DEPS
if (NOT STATIC_DEPS STREQUAL "all" AND NOT STATIC_DEPS STREQUAL "none")
	foreach(dep ${STATIC_DEPS})
		list(FIND external_deps ${dep} idx)
		if(idx EQUAL -1)
			message(FATAL_ERROR "Invalid STATIC_DEPS. There is no ${dep} in the list of ${external_deps}")
		endif()
	endforeach()
endif()

# Validate WITH_NDLLS
foreach(ndll ${WITH_NDLLS})
	list(FIND ndll_targets ${ndll} idx)
	if(idx EQUAL -1)
		message(FATAL_ERROR "Invalid WITH_NDLLS. There is no ${ndll} in the list of ${idx}.")
	endif()
endforeach()
foreach(ndll ${core_ndll_targets})
	list(FIND WITH_NDLLS ${ndll} idx)
	if(idx EQUAL -1)
		message(FATAL_ERROR "${ndll} must be built. Add it to WITH_NDLLS.")
	endif()
endforeach()

# Set STATIC_* variables according to STATIC_DEPS.
foreach(dep ${external_deps})
	string(TOUPPER ${dep} var)
	if (STATIC_DEPS STREQUAL "all")
		set(STATIC_${var} TRUE)
	elseif (STATIC_DEPS STREQUAL "none")
		set(STATIC_${var} FALSE)
	else()
		list(FIND STATIC_DEPS ${dep} static_idx)
		if (static_idx EQUAL -1)
			set(STATIC_${var} FALSE)
		else()
			set(STATIC_${var} TRUE)
		endif()
	endif()
endforeach()

# Set EXCLUDE_*_NDLL_FROM_ALL variables according to WITH_NDLLS.
foreach(ndll ${ndll_targets})
	string(REPLACE . _ var ${ndll})
	string(TOUPPER ${var} var)
	list(FIND WITH_NDLLS ${ndll} ndll_idx)
	if (ndll_idx EQUAL -1)
		set(EXCLUDE_${var}_FROM_ALL EXCLUDE_FROM_ALL)
	else()
		set(EXCLUDE_${var}_FROM_ALL "")
	endif()
endforeach()

include(ExternalProject)


if (RELOCATABLE)
	# https://cmake.org/Wiki/CMake_RPATH_handling
	set(CMAKE_SKIP_BUILD_RPATH FALSE)
	set(CMAKE_SKIP_INSTALL_RPATH FALSE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	if(APPLE)
		set(CMAKE_MACOSX_RPATH TRUE)
		set(CMAKE_INSTALL_RPATH @executable_path/)
	elseif(UNIX)
		set(CMAKE_INSTALL_RPATH \$ORIGIN)
	endif()
endif()

if(UNIX AND NOT APPLE)
	add_definitions(-DABI_ELF)
endif()

if(UNIX)
	add_definitions(-D_GNU_SOURCE)
	add_compile_options(-fno-omit-frame-pointer -fPIC)

	# https://github.com/HaxeFoundation/neko/pull/17
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		add_compile_options(-mincoming-stack-boundary=2)
	endif()

	find_package(PkgConfig REQUIRED)
endif()

# git is used for source_archive and for applying patch
find_package(Git REQUIRED)

# ExternalProject configs
set(EP_CONFIGS
	PREFIX ${CMAKE_BINARY_DIR}/libs
)
if(${CMAKE_VERSION} VERSION_GREATER 3.0)
	list(APPEND EP_CONFIGS
		DOWNLOAD_NO_PROGRESS 1
	)
endif()
set(EP_PROPS
	EXCLUDE_FROM_ALL 1
)


include_directories(
	vm
	libs/common
)

add_library(libneko SHARED
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

add_library(std.ndll MODULE
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

add_executable(nekovm
	vm/stats.c
	vm/main.c
)

if (STATIC_BOEHMGC)
	if (WIN32)
		ExternalProject_Add(BoehmGC
			${EP_CONFIGS}
			URL http://www.hboehm.info/gc/gc_source/gc-7.1.tar.gz
			URL_MD5 2ff9924c7249ef7f736ecfe6f08f3f9b
			CMAKE_ARGS
				-Wno-dev
				-Denable_threads=ON
				-DCMAKE_USE_WIN32_THREADS_INIT=ON
			PATCH_COMMAND
				${CMAKE_COMMAND} -E copy
					${CMAKE_SOURCE_DIR}/libs/include/gc/CMakeLists.txt
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC/CMakeLists.txt
				&& cd ${CMAKE_BINARY_DIR}/libs/src/BoehmGC &&
				${GIT_EXECUTABLE} apply --ignore-space-change --ignore-whitespace --whitespace=nowarn --unsafe-paths ${CMAKE_SOURCE_DIR}/libs/include/gc/av_safety.patch
			INSTALL_COMMAND
				${CMAKE_COMMAND} -E copy_directory
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC/include
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include/gc
		)
		set(GC_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include)
		set(GC_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/${CMAKE_CFG_INTDIR}/gcmt-dll.lib
		)
		add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gcmt-dll.dll
			DEPENDS BoehmGC
			COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/${CMAKE_CFG_INTDIR}/gcmt-dll.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		)
		add_custom_target(gcmt-dll.dll ALL
			DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gcmt-dll.dll
		)
		add_dependencies(nekovm gcmt-dll.dll)
	else()
		ExternalProject_Add(BoehmGC
			${EP_CONFIGS}
			URL http://hboehm.info/gc/gc_source/gc-7.4.2.tar.gz
			URL_MD5 12c05fd2811d989341d8c6d81f66af87
			PATCH_COMMAND
				${CMAKE_COMMAND} -Durl=http://www.ivmaisoft.com/_bin/atomic_ops/libatomic_ops-7.4.2.tar.gz -Dfile=${CMAKE_BINARY_DIR}/libs/src/BoehmGC/libatomic_ops-7.4.2.tar.gz -P ${CMAKE_SOURCE_DIR}/cmake/download.cmake &&
				${CMAKE_COMMAND} -E tar x ${CMAKE_BINARY_DIR}/libs/src/BoehmGC/libatomic_ops-7.4.2.tar.gz &&
				${CMAKE_COMMAND} -E rename libatomic_ops-7.4.2 libatomic_ops
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/BoehmGC &&
				./configure
					--prefix=${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build
					--enable-threads=posix
					--with-pic
					--enable-shared=no
					--enable-static=yes
					--enable-silent-rules
					--silent
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/BoehmGC &&
				make CFLAGS=-w
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/BoehmGC && 
				make install
		)
		set(GC_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include)
		set(GC_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/lib/libgc.a
		)
	endif()
	set_target_properties(BoehmGC PROPERTIES ${EP_PROPS})
	add_dependencies(libneko BoehmGC)
else()
	find_package(BoehmGC REQUIRED)
endif()

target_include_directories(libneko PRIVATE ${GC_INCLUDE_DIRS})

target_link_libraries(libneko ${GC_LIBRARIES})
target_link_libraries(std.ndll libneko)
target_link_libraries(nekovm libneko)

if(WIN32)
	target_link_libraries(std.ndll ws2_32)
endif()

if(UNIX)
	find_package(Threads)
	target_link_libraries(libneko dl m ${CMAKE_THREAD_LIBS_INIT})
endif()

set_target_properties(nekovm
	PROPERTIES
	OUTPUT_NAME neko
)

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
# OpenSSL

if (STATIC_OPENSSL)
	if (APPLE)
		if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "i386")
			set(OPENSSL_CONF ./Configure darwin-i386-cc)
		elseif (${CMAKE_OSX_ARCHITECTURES} STREQUAL "x86_64")
			set(OPENSSL_CONF ./Configure darwin64-x86_64-cc)
		endif()
	else()
		set(OPENSSL_CONF ./config)
	endif()
	if (WIN32)
		# perl is needed to run the openssl Configure script...
		find_package(Perl REQUIRED)
		set(OPENSSL_CONFS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				${PERL_EXECUTABLE} Configure VC-WIN32 no-asm --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix &&
				call ms/do_ms.bat
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				nmake /S -f ms/nt.mak
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL && 
				nmake /S -f ms/nt.mak install
		)
	else()
		set(OPENSSL_CONFS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				${OPENSSL_CONF} --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				make depend && make
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL && 
				make install
		)
	endif()
	ExternalProject_Add(OpenSSL
		${EP_CONFIGS}
		URL https://www.openssl.org/source/openssl-1.0.2g.tar.gz
		URL_MD5 f3c710c045cdee5fd114feb69feba7aa
		${OPENSSL_CONFS}
	)
	set_target_properties(OpenSSL PROPERTIES ${EP_PROPS})
	if (WIN32)
		set(OPENSSL_LIBRARIES
			Crypt32
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libeay32.lib
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/ssleay32.lib
		)
	endif()
endif()

######################
# zlib.ndll

add_library(zlib.ndll MODULE ${EXCLUDE_ZLIB_NDLL_FROM_ALL} libs/zlib/zlib.c)


if (STATIC_ZLIB)
	set(ZLIB_CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
		-Wno-dev
	)
	if (UNIX)
		list(APPEND ZLIB_CMAKE_ARGS
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DCMAKE_C_FLAGS=-fPIC
		)
	endif()
	ExternalProject_Add(Zlib
		${EP_CONFIGS}
		URL http://zlib.net/zlib-1.2.8.tar.gz
		URL_MD5 44d667c142d7cda120332623eab69f40
		CMAKE_ARGS ${ZLIB_CMAKE_ARGS}
	)
	set_target_properties(Zlib PROPERTIES ${EP_PROPS})
	set(ZLIB_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/install-prefix/include)
	if (WIN32)
		set(ZLIB_LIBRARIES
			optimized ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/zlibstatic.lib
			debug ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/zlibstaticd.lib
		)
	else()
		set(ZLIB_LIBRARIES ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libz.a)
	endif()
	add_dependencies(zlib.ndll Zlib)
else()
	pkg_check_modules(ZLIB REQUIRED zlib)
endif()

target_include_directories(zlib.ndll PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(zlib.ndll libneko ${ZLIB_LIBRARIES})

set_target_properties(zlib.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME zlib
	SUFFIX .ndll
)

######################
# mysql.ndll

add_library(mysql.ndll MODULE ${EXCLUDE_MYSQL_NDLL_FROM_ALL} libs/mysql/mysql.c)

if (STATIC_MARIADBCONNECTOR)
	if (STATIC_OPENSSL)
		set(OPENSSL_CONF -DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(OPENSSL_DEP OpenSSL)
	elseif()
		set(OPENSSL_CONF "")
		set(OPENSSL_DEP "")
	endif()
	ExternalProject_Add(MariaDBConnector
		${EP_CONFIGS}
		DEPENDS ${OPENSSL_DEP}
		URL https://downloads.mariadb.org/f/connector-c-2.2.2/mariadb-connector-c-2.2.2-src.tar.gz
		URL_MD5 a9767a8ed295f3d0ca3d9241ccde4c3f
		CMAKE_ARGS
			-Wno-dev
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DWITH_OPENSSL=ON
			${OPENSSL_CONF}
		PATCH_COMMAND ${CMAKE_COMMAND} -Dmariadb_source=${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector -P ${CMAKE_SOURCE_DIR}/cmake/patch_mariadb.cmake
		BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build &&
			${CMAKE_COMMAND} --build . --target mariadbclient --config ${CMAKE_CFG_INTDIR}
		INSTALL_COMMAND echo skip install
	)
	set_target_properties(MariaDBConnector PROPERTIES ${EP_PROPS})
	set(MARIADB_CONNECTOR_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector/include)
	if (WIN32)
		set(MARIADB_CONNECTOR_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/${CMAKE_CFG_INTDIR}/mariadbclient.lib
		)
	else()
		set(MARIADB_CONNECTOR_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/libmariadbclient.a
		)
	endif()
	add_dependencies(mysql.ndll MariaDBConnector)
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

target_link_libraries(mysql.ndll libneko ${OPENSSL_LIBRARIES} ${MARIADB_CONNECTOR_LIBRARIES})

set_target_properties(mysql.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mysql
	SUFFIX .ndll
)

######################
# mysql5.ndll

add_library(mysql5.ndll MODULE ${EXCLUDE_MYSQL5_NDLL_FROM_ALL}
	libs/common/sha1.c
	libs/common/socket.c
	libs/mysql/my_proto/my_proto.c
	libs/mysql/my_proto/my_api.c
	libs/mysql/mysql.c
)

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

add_library(regexp.ndll MODULE ${EXCLUDE_REGEXP_NDLL_FROM_ALL} libs/regexp/regexp.c)
if (STATIC_PCRE)
	if (WIN32)
		ExternalProject_Add(PCRE
			${EP_CONFIGS}
			URL http://downloads.sourceforge.net/project/pcre/pcre/8.38/pcre-8.38.tar.gz
			URL_MD5 8a353fe1450216b6655dfcf3561716d9
			CMAKE_ARGS
				-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				-Wno-dev
				-DPCRE_BUILD_PCRECPP=OFF
				-DPCRE_BUILD_PCREGREP=OFF
				-DPCRE_BUILD_TESTS=OFF
				-DPCRE_SUPPORT_JIT=ON
				-DPCRE_SUPPORT_UNICODE_PROPERTIES=ON
		)
		set(PCRE_LIBRARIES
			optimized ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/pcre.lib
			debug ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/pcred.lib
		)
	else()
		ExternalProject_Add(PCRE
			${EP_CONFIGS}
			URL ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-8.38.tar.gz
			URL_MD5 8a353fe1450216b6655dfcf3561716d9
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/PCRE &&
				./configure
					--prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					--with-pic
					--enable-unicode-properties
					--enable-silent-rules
					--enable-jit
					--disable-cpp
					--enable-shared=no
					--enable-static=yes
					--silent
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/PCRE &&
				make CFLAGS=-w
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/PCRE &&
				make install
		)
		set(PCRE_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libpcre.a
		)
	endif()
	set_target_properties(PCRE PROPERTIES ${EP_PROPS})
	set(PCRE_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/install-prefix/include)
	add_dependencies(regexp.ndll PCRE)
else()
	find_package(PCRE REQUIRED)
endif()

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

if (STATIC_SQLITE3)
	if (NOT EXISTS ${CMAKE_BINARY_DIR}/libs/src/Sqlite3)
		message(STATUS "Downloading sqlite...")
		file(DOWNLOAD https://www.sqlite.org/2016/sqlite-autoconf-3120100.tar.gz ${CMAKE_BINARY_DIR}/libs/src/sqlite-autoconf-3120100.tar.gz)
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar x sqlite-autoconf-3120100.tar.gz
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/libs/src
		)
		file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/libs/src/Sqlite3)
		file(RENAME ${CMAKE_BINARY_DIR}/libs/src/sqlite-autoconf-3120100 ${CMAKE_BINARY_DIR}/libs/src/Sqlite3)
		message(STATUS "Downloading sqlite... done")
	endif()

	add_library(sqlite.ndll MODULE ${EXCLUDE_SQLITE_NDLL_FROM_ALL}
		libs/sqlite/sqlite.c
		${CMAKE_BINARY_DIR}/libs/src/Sqlite3/sqlite3.c
	)
	target_include_directories(sqlite.ndll PRIVATE ${CMAKE_BINARY_DIR}/libs/src/Sqlite3)
	target_link_libraries(sqlite.ndll libneko)
	target_compile_definitions(sqlite.ndll PRIVATE SQLITE_MAX_VARIABLE_NUMBER=250000 SQLITE_ENABLE_RTREE=1)
else()
	add_library(sqlite.ndll MODULE ${EXCLUDE_SQLITE_NDLL_FROM_ALL} libs/sqlite/sqlite.c)
	pkg_check_modules(SQLITE3 REQUIRED sqlite3)
	target_include_directories(sqlite.ndll PRIVATE ${SQLITE3_INCLUDE_DIRS})
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

add_library(ui.ndll MODULE ${EXCLUDE_UI_NDLL_FROM_ALL} libs/ui/ui.c)

target_link_libraries(ui.ndll libneko)

if(APPLE)
	find_library(CARBON_LIBRARY Carbon REQUIRED)
	target_link_libraries(ui.ndll ${CARBON_LIBRARY})
elseif(UNIX)
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

add_library(mod_neko2.ndll MODULE ${EXCLUDE_MOD_NEKO2_NDLL_FROM_ALL}
	vm/stats.c
	libs/mod_neko/mod_neko.c
	libs/mod_neko/cgi.c
)


if (STATIC_APACHE)
	if (STATIC_OPENSSL)
		set(OPENSSL_CONF --with-openssl=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(OPENSSL_DEP OpenSSL)
	elseif()
		set(OPENSSL_CONF "")
		set(OPENSSL_DEP "")
	endif()
	if (STATIC_APR)
		set(APR_CONF --with-apr=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(APR_DEP APR)
	elseif()
		set(APR_CONF "")
		set(APR_DEP "")
	endif()
	if (STATIC_APRUTIL)
		set(APRUTIL_CONF --with-apr-util=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(APRUTIL_DEP APRutil)
	elseif()
		set(APRUTIL_CONF "")
		set(APRUTIL_DEP "")
	endif()
	if (STATIC_PCRE)
		set(PCRE_CONF --with-pcre=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(PCRE_DEP PCRE)
	elseif()
		set(PCRE_CONF "")
		set(PCRE_DEP "")
	endif()
	if (STATIC_ZLIB)
		set(ZLIB_CONF --with-z=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(ZLIB_DEP Zlib)
	elseif()
		set(ZLIB_CONF "")
		set(ZLIB_DEP "")
	endif()

	if(WIN32)
		set(APR_CONFIGS
			CMAKE_ARGS
				-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				-Wno-dev
				-DAPR_INSTALL_PRIVATE_H=ON
		)
	else()
		set(APR_CONFIGS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APR &&
				./configure --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				--enable-shared=no
				--enable-static=yes
				--silent
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APR &&
				make CFLAGS=-w
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APR && 
				make install
		)
	endif()
	ExternalProject_Add(APR
		${EP_CONFIGS}
		URL http://archive.apache.org/dist/apr/apr-1.5.2.tar.gz
		URL_MD5 98492e965963f852ab29f9e61b2ad700
		${APR_CONFIGS}
	)
	set_target_properties(APR PROPERTIES ${EP_PROPS})


	if(WIN32)
		set(APRutil_CONFIGS
			CMAKE_ARGS
				-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				-Wno-dev
				-DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix
		)
	else()
		set(APRutil_CONFIGS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
				./configure
					--prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					--silent
					${APR_CONF}
					${OPENSSL_CONF}
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
				make CFLAGS=-w
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil && 
				make install
		)
	endif()
	ExternalProject_Add(APRutil
		${EP_CONFIGS}
		DEPENDS ${APR_DEP} ${OPENSSL_DEP}
		URL http://archive.apache.org/dist/apr/apr-util-1.5.4.tar.gz
		URL_MD5 866825c04da827c6e5f53daff5569f42
		${APRutil_CONFIGS}
	)
	set_target_properties(APRutil PROPERTIES ${EP_PROPS})


	if(WIN32)
		set(Apache_CONFIGS
			CMAKE_ARGS
				-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				-Wno-dev
				-DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				"-DEXTRA_COMPILE_FLAGS=/D PCRE_STATIC"
				-DEXTRA_LIBS=Ws2_32
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/Apache-build &&
				${CMAKE_COMMAND} --build . --target libhttpd --config ${CMAKE_CFG_INTDIR}
			INSTALL_COMMAND echo skip install
		)
		set(APACHE_INCLUDE_DIRS
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/include
			${CMAKE_BINARY_DIR}/libs/src/Apache/include
			${CMAKE_BINARY_DIR}/libs/src/Apache/os/win32
			${CMAKE_BINARY_DIR}/libs/src/Apache-build
		)
		set(APACHE_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libapr-1.lib
			${CMAKE_BINARY_DIR}/libs/src/Apache-build/${CMAKE_CFG_INTDIR}/libhttpd.lib
		)
	else()
		set(Apache_CONFIGS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/Apache &&
				./configure
					--prefix=${CMAKE_BINARY_DIR}/libs/src/Apache-build
					--silent
					${APR_CONF}
					${APRUTIL_CONF}
					${OPENSSL_CONF}
					${PCRE_CONF}
					${ZLIB_CONF}
			BUILD_COMMAND echo skip build
			INSTALL_COMMAND echo skip install
		)
		set(APACHE_INCLUDE_DIRS
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/include/apr-1
			${CMAKE_BINARY_DIR}/libs/src/Apache/include
			${CMAKE_BINARY_DIR}/libs/src/Apache/os/unix
		)
		set(APACHE_LIBRARIES

		)
	endif()
	ExternalProject_Add(Apache
		${EP_CONFIGS}
		DEPENDS ${APR_DEP} ${APRUTIL_DEP} ${OPENSSL_DEP} ${PCRE_DEP}
		URL http://archive.apache.org/dist/httpd/httpd-2.4.20.tar.gz
		URL_MD5 e725c268624737a163dc844e28f720d1
		${Apache_CONFIGS}
	)
	set_target_properties(Apache PROPERTIES ${EP_PROPS})
	add_dependencies(mod_neko2.ndll Apache APR APRutil)
else()
	find_package(APACHE REQUIRED)
	find_package(APR REQUIRED)
	set(APACHE_LIBRARIES ${APR_LIBRARIES} ${APRUTIL_LIBRARIES})
	set(APACHE_INCLUDE_DIRS ${APACHE_INCLUDE_DIR} ${APR_INCLUDE_DIR} ${APRUTIL_INCLUDE_DIR})
endif()

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

add_library(mod_tora2.ndll MODULE ${EXCLUDE_MOD_TORA2_NDLL_FROM_ALL}
	libs/common/socket.c
	libs/mod_tora/protocol.c
	libs/mod_tora/mod_tora.c
)

add_dependencies(mod_tora2.ndll mod_neko2.ndll)

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

add_library(ssl.ndll MODULE ${EXCLUDE_SSL_NDLL_FROM_ALL} libs/ssl/ssl.c)

if (STATIC_MBEDTLS)
	set(MBEDTLS_CMAKE_ARGS
		-Wno-dev
		-DENABLE_PROGRAMS=OFF
		-DENABLE_TESTING=OFF
		-DUSE_STATIC_MBEDTLS_LIBRARY=ON
	)
	if (UNIX)
		list(APPEND MBEDTLS_CMAKE_ARGS
			-DLINK_WITH_PTHREAD=ON
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DCMAKE_C_FLAGS=-fPIC
		)
	endif()
	ExternalProject_Add(MbedTLS
		${EP_CONFIGS}
		URL https://tls.mbed.org/download/mbedtls-2.2.1-apache.tgz
		URL_MD5 77751c0e370ed2ab01934e4c5e1d380f
		CMAKE_ARGS ${MBEDTLS_CMAKE_ARGS}
		PATCH_COMMAND ${CMAKE_COMMAND} -Dsource=${CMAKE_SOURCE_DIR} -DMbedTLS_source=${CMAKE_BINARY_DIR}/libs/src/MbedTLS -P ${CMAKE_SOURCE_DIR}/cmake/patch_mbedtls.cmake
		INSTALL_COMMAND echo skip install
	)
	set_target_properties(MbedTLS PROPERTIES ${EP_PROPS})
	set(MBEDTLS_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libs/src/MbedTLS/include)

	if (WIN32)
		set(MBEDTLS_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MbedTLS-build/library/${CMAKE_CFG_INTDIR}/mbedx509.lib
			${CMAKE_BINARY_DIR}/libs/src/MbedTLS-build/library/${CMAKE_CFG_INTDIR}/mbedtls.lib
			${CMAKE_BINARY_DIR}/libs/src/MbedTLS-build/library/${CMAKE_CFG_INTDIR}/mbedcrypto.lib
		)
		target_link_libraries(ssl.ndll ws2_32 Advapi32 Crypt32)
	else()
		set(MBEDTLS_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MbedTLS-build/library/libmbedx509.a
			${CMAKE_BINARY_DIR}/libs/src/MbedTLS-build/library/libmbedtls.a
			${CMAKE_BINARY_DIR}/libs/src/MbedTLS-build/library/libmbedcrypto.a
		)
	endif()
	add_dependencies(ssl.ndll MbedTLS)
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

target_link_libraries(ssl.ndll libneko ${MBEDTLS_LIBRARIES})

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
elseif(CMAKE_HOST_APPLE)
	set(set_neko_env "")
	set(neko_exec DYLD_FALLBACK_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} NEKOPATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} $<TARGET_FILE:nekovm>)
else()
	set(set_neko_env "")
	set(neko_exec LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} NEKOPATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} $<TARGET_FILE:nekovm>)
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
	COMMAND ${GIT_EXECUTABLE} archive -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name} HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(source_archive
	DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name}
)

#######################

# install target

if (WIN32)
	set(DEST_BIN .)
	set(DEST_LIB .)
	set(DEST_NDLL .)

	include(InstallRequiredSystemLibraries)
	install (
		FILES
			${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
			${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gcmt-dll.dll
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
	TARGETS ${WITH_NDLLS}
	DESTINATION ${DEST_NDLL}
)
install (
	DIRECTORY vm/
	DESTINATION include
	FILES_MATCHING PATTERN "neko*.h"
)

if (RUN_LDCONFIG)
	install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/ldconfig.cmake")
endif()

install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/flatten.cmake")

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
else()
	set(CPACK_GENERATOR "TGZ")
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
