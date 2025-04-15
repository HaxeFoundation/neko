cmake_minimum_required(VERSION 3.10.2)

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.24)
	cmake_policy(VERSION 3.24)
elseif (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.19)
	cmake_policy(VERSION 3.19)
endif()

project(Neko C)

include(GNUInstallDirs)
include(CheckCCompilerFlag)
include(CheckIncludeFile)
include(TestBigEndian)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  # FreeBSD puts all thirdparty libraries in /usr/local
  link_directories(/usr/local/lib)
endif()

# put output in "bin"

set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# avoid the extra "Debug", "Release" directories
# https://stackoverflow.com/questions/7747857/in-cmake-how-do-i-work-around-the-debug-and-release-directories-visual-studio-2
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${OUTPUT_DIR} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# Make sure CMAKE_INSTALL_LIBDIR is relative
if(IS_ABSOLUTE ${CMAKE_INSTALL_LIBDIR})
	file(RELATIVE_PATH CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_LIBDIR})
endif()

set(NEKO_VERSION_MAJOR 2)
set(NEKO_VERSION_MINOR 4)
set(NEKO_VERSION_PATCH 1)

string(TIMESTAMP NEKO_BUILD_YEAR "%Y")

# NEKO_VERSION is cached such that we can query it by `cmake -L -N -B . | grep NEKO_VERSION`
set(NEKO_VERSION ${NEKO_VERSION_MAJOR}.${NEKO_VERSION_MINOR}.${NEKO_VERSION_PATCH} CACHE STRING INTERNAL)

# Determine target endianness
TEST_BIG_ENDIAN(NEKO_BIG_ENDIAN)

option(WITH_REGEXP "Build with Perl-compatible regex support." ON)
option(WITH_UI "Build with GTK-3 UI support." ON)
option(WITH_SSL "Build with SSL support." ON)
option(WITH_MYSQL "Build with MySQL support." ON)
option(WITH_SQLITE "Build with SQLite support." ON)
option(WITH_APACHE "Build with Apache modules." ON)
option(WITH_NEKOML "Build NekoML." ON)

# Process common headers in libraries
# TODO libraries should not be built from this file, but rather by traversing the tree using add_subdirectory
#add_subdirectory(libs)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(arch_64 "64")
else()
	set(arch_64 "")
endif()

if(WIN32)
	if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		set (CMAKE_INSTALL_PREFIX "C:/HaxeToolkit/neko" CACHE PATH "default install path" FORCE)
	endif()
	set(NEKO_MODULE_PATH ${CMAKE_INSTALL_PREFIX})
else()
	set(NEKO_MODULE_PATH ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/neko)
endif()

if(APPLE AND STATIC_DEPS STREQUAL "all")
	if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "11" CACHE STRING "CMAKE_OSX_DEPLOYMENT_TARGET" FORCE)
	endif()
endif()

check_include_file(xlocale.h NEKO_XLOCALE_H)

option(NEKO_JIT_DISABLE "Disable Neko JIT." OFF)
option(NEKO_JIT_DEBUG "Debug Neko JIT." OFF)

configure_file (
	"${CMAKE_SOURCE_DIR}/vm/neko.h.in"
	"${CMAKE_BINARY_DIR}/neko.h"
)

set(external_deps
	BoehmGC
	Zlib
	OpenSSL
	MariaDBConnector
	pcre2
	SQLite3
	APR
	APRutil
	Apache
	MbedTLS
)

if (WIN32)
	set(STATIC_DEPS_DEFAULT "all")
else()
	set(STATIC_DEPS_DEFAULT "none")

	option(RELOCATABLE "Set RPATH to $ORIGIN (Linux) / @executable_path (Mac)." ON)

	if (NOT APPLE)
		option(RUN_LDCONFIG "Run ldconfig after install." ON)
	endif()
endif()

set(STATIC_DEPS_DOC "Dependencies that should be linked statically. Can be \"all\", \"none\", or a list of library names (e.g. \"${external_deps}\").")
set(STATIC_DEPS ${STATIC_DEPS_DEFAULT} CACHE STRING "${STATIC_DEPS_DOC}")

# Validate STATIC_DEPS
if (STATIC_DEPS STREQUAL "all")
	set(STATIC_DEPS_ALL ${external_deps})
	if (WIN32)
		list(REMOVE_ITEM STATIC_DEPS_ALL BoehmGC)
	endif()
	set(STATIC_DEPS ${STATIC_DEPS_ALL} CACHE STRING "${STATIC_DEPS_DOC}" FORCE)
elseif (STATIC_DEPS STREQUAL "none")
	message(STATUS "set STATIC_DEPS to nothing")
	set(STATIC_DEPS CACHE STRING "${STATIC_DEPS_DOC}" FORCE)
else()
	foreach(dep ${STATIC_DEPS})
		list(FIND external_deps ${dep} idx)
		if(idx EQUAL -1)
			message(FATAL_ERROR "Invalid STATIC_DEPS. There is no ${dep} in the list of ${external_deps}")
		elseif(WIN32 AND dep STREQUAL "BoehmGC")
			message(FATAL_ERROR "Cannot link ${dep} statically on Windows")
		endif()
	endforeach()
endif()

# Set STATIC_* variables according to STATIC_DEPS.
foreach(dep ${external_deps})
	string(TOUPPER ${dep} var)
	list(FIND STATIC_DEPS ${dep} static_idx)
	if (static_idx EQUAL -1)
		set(STATIC_${var} FALSE)
	else()
		set(STATIC_${var} TRUE)
	endif()
endforeach()

include(ExternalProject)

if (RELOCATABLE)
	# https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling
	# Set this to true, otherwise the binaries won't be relocatable until after installation:
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	if(APPLE)
		set(CMAKE_INSTALL_RPATH @executable_path/)
	elseif(UNIX)
		set(CMAKE_INSTALL_RPATH \$ORIGIN)
	endif()
endif()

list(APPEND CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})

if(UNIX AND NOT APPLE)
	add_definitions(-DABI_ELF)
endif()

if(UNIX)
	add_definitions(-D_GNU_SOURCE)
	add_compile_options(-fno-omit-frame-pointer)

	set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
	set(ARG_PIC -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE)

	# https://github.com/HaxeFoundation/neko/pull/17
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		check_c_compiler_flag(-mincoming-stack-boundary=2 HAS_MINCOMING_STACK_BOUNDARY)
		check_c_compiler_flag(-mstack-alignment=2 HAS_MSTACK_ALIGNMENT)
		if(HAS_MINCOMING_STACK_BOUNDARY)
			add_compile_options(-mincoming-stack-boundary=2)
		elseif(HAS_MSTACK_ALIGNMENT)
			add_compile_options(-mstack-alignment=2)
		endif()
	endif()

	find_package(PkgConfig REQUIRED)
endif()

# git is used for source_archive and for applying patch
find_package(Git REQUIRED)

# copy the lib/src folder to build directory
# (if it is a fat archive, there will be external libraries)
if(EXISTS ${CMAKE_SOURCE_DIR}/libs/src)
	file(COPY ${CMAKE_SOURCE_DIR}/libs/src DESTINATION ${CMAKE_BINARY_DIR}/libs)
endif()

# ExternalProject configs
set(EP_CONFIGS
	PREFIX ${CMAKE_BINARY_DIR}/libs
	DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/libs/download
)
list(APPEND EP_CONFIGS
	DOWNLOAD_NO_PROGRESS 1
)
if (${CMAKE_VERSION} VERSION_LESS 3.19)
	list(APPEND EP_CONFIGS
		INDEPENDENT_STEP_TARGETS download
	)
else()
	# download is independent by default in 3.19
	list(APPEND EP_CONFIGS
		STEP_TARGETS download
	)
endif()
set(EP_PROPS
	EXCLUDE_FROM_ALL 1
)


include_directories(
	${CMAKE_BINARY_DIR}
	vm
	libs/common
)

file(GLOB libneko_public_headers
	vm/neko*.h
)
list(APPEND libneko_public_headers
	${CMAKE_BINARY_DIR}/neko.h
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

add_executable(nekovm
	vm/stats.c
	vm/main.c
)

# We need to build from source on Windows regardless
if (STATIC_BOEHMGC OR WIN32)
	ExternalProject_Add(libatomic_ops
		${EP_CONFIGS}
		URL https://github.com/ivmai/libatomic_ops/releases/download/v7.6.14/libatomic_ops-7.6.14.tar.gz
		URL_MD5 ee8251f5091b7938d18be4dda843a515
		CONFIGURE_COMMAND echo skip config
		BUILD_COMMAND echo skip build
		INSTALL_COMMAND echo skip install
	)
	set_target_properties(libatomic_ops PROPERTIES ${EP_PROPS})

	set (
		BoehmGC_CONFIGS
		DEPENDS libatomic_ops
		URL https://github.com/ivmai/bdwgc/releases/download/v7.6.16/gc-7.6.16.tar.gz
		URL_MD5 74fb76b6bccf0874cec65b794535a7dd
	)

	set(GC_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include)

	if (WIN32)
		set(GC_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/${CMAKE_CFG_INTDIR}/gcmt-dll.lib
		)
		ExternalProject_Add(BoehmGC
			${EP_CONFIGS}
			${BoehmGC_CONFIGS}
			CMAKE_ARGS
				-Wno-dev
				-Denable_threads=ON
				-Denable_parallel_mark=OFF
				-DCMAKE_USE_WIN32_THREADS_INIT=ON
				-DCMAKE_CXX_STANDARD=14
			PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/libs/src/libatomic_ops ${CMAKE_BINARY_DIR}/libs/src/BoehmGC/libatomic_ops
			INSTALL_COMMAND
				${CMAKE_COMMAND} -E copy_directory
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC/include
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include/gc
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
		if (APPLE)
			set(GC_CFLAGS "-w -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		else()
			set(GC_CFLAGS "-w")
		endif()

		set(GC_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/lib/libgc.a
		)

		ExternalProject_Add(BoehmGC
			${EP_CONFIGS}
			${BoehmGC_CONFIGS}
			PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/libs/src/libatomic_ops ${CMAKE_BINARY_DIR}/libs/src/BoehmGC/libatomic_ops
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
				make "CFLAGS=${GC_CFLAGS}"
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/BoehmGC &&
				make install
			BYPRODUCTS
				${GC_LIBRARIES}
		)
	endif()

	set_target_properties(BoehmGC PROPERTIES ${EP_PROPS})
	add_dependencies(libneko BoehmGC)
else()
	find_package(BoehmGC REQUIRED)
endif()

add_custom_target(download_deps)
if (STATIC_BOEHMGC OR WIN32)
	add_dependencies(download_deps libatomic_ops-download)
	add_dependencies(download_deps BoehmGC-download)
endif()

target_include_directories(libneko PRIVATE ${GC_INCLUDE_DIR})

target_link_libraries(libneko ${GC_LIBRARIES})
target_link_libraries(nekovm libneko)

if(UNIX)
	find_package(Threads)
	target_link_libraries(libneko ${CMAKE_DL_LIBS} m ${CMAKE_THREAD_LIBS_INIT})
endif()

set_target_properties(nekovm libneko
	PROPERTIES
	OUTPUT_NAME neko
)

set_target_properties(libneko
	PROPERTIES
	VERSION ${NEKO_VERSION}
	SOVERSION ${NEKO_VERSION_MAJOR}
	COMPILE_DEFINITIONS "_USRDLL;NEKOVM_DLL_EXPORTS;NEKO_SOURCES"
	PUBLIC_HEADER "${libneko_public_headers}"
	PDB_NAME libneko
)

#######################

# compilers
# nekoc, nekoml, nekotools, and test.n

if (CMAKE_HOST_WIN32)
	set(set_neko_env set NEKOPATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
	set(neko_exec $<TARGET_FILE:nekovm>)
else()
	set(set_neko_env "")
	set(neko_exec NEKOPATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY} $<TARGET_FILE:nekovm>)
endif()

file(GLOB compilers_src
	src/neko/*.nml
	src/nekoml/*.nml
	boot/*.n
)

if (RECOMPILE_NEKOC_NEKOML)
	add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		COMMAND ${set_neko_env}

		COMMAND ${neko_exec} ../boot/nekoml.n -nostd neko/Main.nml nekoml/Main.nml
		COMMAND ${neko_exec} ../boot/nekoc.n -link ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n neko/Main
		COMMAND ${neko_exec} ../boot/nekoc.n -link ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n nekoml/Main

		VERBATIM
		DEPENDS nekovm std.ndll ${compilers_src}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
	)
else()
	add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		COMMAND ${CMAKE_COMMAND} -E copy ../boot/nekoc.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		COMMAND ${CMAKE_COMMAND} -E copy ../boot/nekoml.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}

		DEPENDS std.ndll
		VERBATIM
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
	)
endif()

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test.n
	COMMAND ${set_neko_env}
	COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n tools/test.neko
	COMMAND ${CMAKE_COMMAND} -E copy tools/test.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	COMMAND ${CMAKE_COMMAND} -E remove tools/test.n
	VERBATIM
	DEPENDS nekovm std.ndll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n ${CMAKE_SOURCE_DIR}/src/tools/test.neko
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)
add_custom_target(test.n ALL DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test.n)

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot.n
	COMMAND ${set_neko_env}
	COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n tools/nekoboot.neko
	COMMAND ${CMAKE_COMMAND} -E copy tools/nekoboot.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	# COMMAND ${CMAKE_COMMAND} -E remove tools/nekoboot.n
	VERBATIM
	DEPENDS nekovm std.ndll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n ${CMAKE_SOURCE_DIR}/src/tools/nekoboot.neko
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

file(GLOB nekotools_src
	src/tools/*.nml
)

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.n
	COMMAND ${set_neko_env}
	COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n -nostd -p tools Tools.nml
	COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n -link ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.n Tools
	VERBATIM
	DEPENDS nekovm std.ndll
		${nekotools_src}
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot.n
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/nekoc.c
	COMMAND ${set_neko_env}
	COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot -c ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.c ${CMAKE_BINARY_DIR}
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.c
	VERBATIM
	DEPENDS nekovm std.ndll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_executable(nekoc ${CMAKE_BINARY_DIR}/nekoc.c)
target_link_libraries(nekoc libneko)

if (WITH_NEKOML)
	add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/nekoml.c
		COMMAND ${set_neko_env}
		COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot -c ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.c ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.c
		VERBATIM
		DEPENDS nekovm std.ndll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
	add_executable(nekoml ${CMAKE_BINARY_DIR}/nekoml.c)
	target_link_libraries(nekoml libneko)
endif()

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/nekotools.c
	COMMAND ${set_neko_env}
	COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot -c ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.n
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.c ${CMAKE_BINARY_DIR}
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.c
	VERBATIM
	DEPENDS nekovm std.ndll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoboot.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekotools.n
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_executable(nekotools ${CMAKE_BINARY_DIR}/nekotools.c)
target_link_libraries(nekotools libneko)

if (WITH_NEKOML)
	file(GLOB CORE_NMLS RELATIVE ${CMAKE_SOURCE_DIR}/src src/core/*.nml)
	set(nekoml_std ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.std)
	add_custom_command(OUTPUT ${nekoml_std}
		COMMAND ${set_neko_env}
		COMMAND ${neko_exec} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n -nostd neko/Main.nml nekoml/Main.nml ${CORE_NMLS} -pack ${nekoml_std}
		VERBATIM
		DEPENDS zlib.ndll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
	)
	add_custom_target(nekoml.std ALL DEPENDS ${nekoml_std})
endif()

#######################

# source_archive
# We create our own source package target instead of using CPack's package_source.
# One reason is that the CPack VS generator doesn't generate package_source target.
# See https://cmake.org/Bug/view.php?id=13058

if (WIN32)
	set(source_archive_format zip)
else()
	set(source_archive_format tar.gz)
endif()

set(source_archive_name_we neko-${NEKO_VERSION}-src)
set(source_archive_name ${source_archive_name_we}.${source_archive_format})

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name}
	COMMAND ${GIT_EXECUTABLE} archive --prefix=${source_archive_name_we}/ -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name} HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	VERBATIM
)

add_custom_target(source_archive
	DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name}
)

# source_archive_fat
# It is source_archive + STATIC_DEPS placed in libs/src.

set(source_archive_fat_name_we neko-${NEKO_VERSION}-src-fat)
set(source_archive_fat_name ${source_archive_fat_name_we}.${source_archive_format})

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_fat_name}
	COMMAND ${CMAKE_COMMAND}
		-Dsource_archive_name_we=${source_archive_name_we}
		-Dsource_archive=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_name}
		-Dsource_archive_fat_name_we=${source_archive_fat_name_we}
		-Dsource_archive_fat_name=${source_archive_fat_name}
		-Dbin_dir=${CMAKE_BINARY_DIR}
		-Dsrc_dir=${CMAKE_SOURCE_DIR}
		-Dlib_src_dir=libs/src
		-P ${CMAKE_SOURCE_DIR}/cmake/source_archive_fat.cmake
	DEPENDS source_archive download_deps
	VERBATIM
)

add_custom_target(source_archive_fat
	DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_fat_name}
)

#######################

# install target

if (WIN32)
	set(DEST_BIN .)
	set(DEST_LIB .)
	set(DEST_NDLL .)
	set(DEST_INCLUDE "include")

	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
	include(InstallRequiredSystemLibraries)
	install (
		FILES
			${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/gcmt-dll.dll
		DESTINATION .
	)
else()
	set(DEST_BIN ${CMAKE_INSTALL_BINDIR})
	set(DEST_LIB ${CMAKE_INSTALL_LIBDIR})
	set(DEST_NDLL ${CMAKE_INSTALL_LIBDIR}/neko) # should match NEKO_MODULE_PATH
	set(DEST_INCLUDE ${CMAKE_INSTALL_INCLUDEDIR})
endif()

install (
	TARGETS
		nekovm
		nekoc
		nekotools
		libneko
	EXPORT NekoTargets
	RUNTIME DESTINATION ${DEST_BIN}
	LIBRARY DESTINATION ${DEST_LIB}
	ARCHIVE DESTINATION ${DEST_LIB}
	PUBLIC_HEADER DESTINATION ${DEST_INCLUDE}
)

if (WITH_NEKOML)
	install (
		TARGETS
			nekoml
		EXPORT NekoTargets
		RUNTIME DESTINATION ${DEST_BIN}
		LIBRARY DESTINATION ${DEST_LIB}
		ARCHIVE DESTINATION ${DEST_LIB}
		PUBLIC_HEADER DESTINATION ${DEST_INCLUDE}
	)
	install (
		FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.std
		DESTINATION ${DEST_NDLL}
	)
endif()

include(CMakePackageConfigHelpers)

if(WIN32 AND NOT CYGWIN)
	set(DEF_INSTALL_CMAKE_DIR CMake)
else()
	set(DEF_INSTALL_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/Neko)
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH
	"Installation directory for CMake files")


export(TARGETS
	nekovm
	nekoc
	nekotools
	libneko
	FILE "${CMAKE_BINARY_DIR}/NekoTargets.cmake")
if (WITH_NEKOML)
	export(TARGETS
		nekoml
		APPEND
		FILE "${CMAKE_BINARY_DIR}/NekoTargets.cmake")
endif()

export(PACKAGE Neko)

# NekoConfig referencing build dir

set(NEKO_INCLUDE_DIRS "${CMAKE_BINARY_DIR};${CMAKE_SOURCE_DIR}/vm")
set(NEKO_TARGETS_FILE "${CMAKE_BINARY_DIR}/NekoTargets.cmake")

configure_package_config_file(
	cmake/NekoConfig.cmake.in
	"${CMAKE_BINARY_DIR}/NekoConfig.cmake"
	INSTALL_DESTINATION ${CMAKE_BINARY_DIR}
	PATH_VARS NEKO_INCLUDE_DIRS NEKO_TARGETS_FILE
)

# NekoConfig referencing install dirs

set(NEKO_INCLUDE_DIRS "${DEST_INCLUDE}")
set(NEKO_TARGETS_FILE "${INSTALL_CMAKE_DIR}/NekoTargets.cmake")

configure_package_config_file(
	cmake/NekoConfig.cmake.in
	"${OUTPUT_DIR}/NekoConfig.cmake"
	INSTALL_DESTINATION "${INSTALL_CMAKE_DIR}"
	PATH_VARS NEKO_INCLUDE_DIRS NEKO_TARGETS_FILE
)


write_basic_package_version_file(
	NekoConfigVersion.cmake
	VERSION ${NEKO_VERSION}
	COMPATIBILITY SameMajorVersion
)

install(FILES
	${OUTPUT_DIR}/NekoConfig.cmake
	${CMAKE_BINARY_DIR}/NekoConfigVersion.cmake
	DESTINATION ${INSTALL_CMAKE_DIR}
	COMPONENT dev
)

install(
	EXPORT NekoTargets
	DESTINATION "${INSTALL_CMAKE_DIR}"
	COMPONENT dev
)

if (RUN_LDCONFIG)
	install(SCRIPT "${CMAKE_SOURCE_DIR}/cmake/ldconfig.cmake")
endif()

# A script to create a flat installation for archive package
set (NEKO_FLATTEN_SCRIPT ${CMAKE_BINARY_DIR}/cmake/flatten.cmake)
configure_file(
	"${CMAKE_SOURCE_DIR}/cmake/flatten.cmake.in"
	${NEKO_FLATTEN_SCRIPT}
	IMMEDIATE @ONLY)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})

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
	set(bin_archive_format zip)
else()
	set(CPACK_GENERATOR "TGZ")
	set(bin_archive_format tar.gz)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(OS_NAME "win")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(OS_NAME "osx")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(OS_NAME "linux")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set(OS_NAME "freebsd")
else()
	message( WARNING "unknown ${CMAKE_SYSTEM_NAME}" )
	set(OS_NAME "")
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
	set(arch_suffix "-arm64")
else()
	set(arch_suffix ${arch_64})
endif()

set(bin_archive_name_we "neko-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${OS_NAME}${arch_suffix}")
set(bin_archive_name "${bin_archive_name_we}.${bin_archive_format}")
set(CPACK_PACKAGE_FILE_NAME ${bin_archive_name_we})
# set(CPACK_SOURCE_PACKAGE_FILE_NAME
# 	"neko-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-src")

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

add_test(NAME nekoc
	COMMAND nekoc
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

if (WITH_NEKOML)
	add_test(NAME nekoml
		COMMAND nekoml
		WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)
endif()

add_test(NAME nekotools
	COMMAND nekotools
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

#######################

# debian source packages

if(UNIX AND NOT APPLE)
	add_custom_target(upload_to_ppa
		COMMAND ${CMAKE_COMMAND}
			-Dsource_dir=${CMAKE_SOURCE_DIR}
			-Dbin_dir=${CMAKE_BINARY_DIR}
			-Dsource_archive=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${source_archive_fat_name}
			-DNEKO_VERSION=${NEKO_VERSION}
			-P ${CMAKE_SOURCE_DIR}/cmake/upload_to_ppa.cmake
		DEPENDS source_archive_fat
	)
endif()


#######################

# chocolatey
if(WIN32)
	add_custom_target(package_choco
		COMMAND ${CMAKE_COMMAND}
			-Dsource_dir=${CMAKE_SOURCE_DIR}
			-Dbin_dir=${CMAKE_BINARY_DIR}
			-Dbin_archive=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${bin_archive_name}
			-Dbin_archive_name_we=${bin_archive_name_we}
			-DNEKO_VERSION=${NEKO_VERSION}
			-P ${CMAKE_SOURCE_DIR}/cmake/package_choco.cmake
		DEPENDS package
	)
endif()

add_subdirectory(libs)
