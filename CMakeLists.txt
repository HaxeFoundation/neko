cmake_minimum_required(VERSION 2.8.7)

include(GNUInstallDirs)
include(CheckCCompilerFlag)
include(TestBigEndian)
project(Neko C)

set(CMAKE_OSX_ARCHITECTURES x86_64)
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
# http://stackoverflow.com/questions/7747857/in-cmake-how-do-i-work-around-the-debug-and-release-directories-visual-studio-2
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

# target_include_directories for old cmake versions
if (${CMAKE_VERSION} VERSION_LESS 2.8.8)
	function(target_include_directories target visibility)
		include_directories(${ARGN})
	endfunction()
elseif (${CMAKE_VERSION} VERSION_LESS 2.8.11)
	function(target_include_directories target visibility)
		get_target_property(DIRS ${target} INCLUDE_DIRECTORIES)
		foreach(PATH ${ARGN})
			get_filename_component(ABS_PATH ${PATH} ABSOLUTE)
			list(APPEND DIRS ${ABS_PATH})
		endforeach()
		set_target_properties(${target}
			PROPERTIES
			INCLUDE_DIRECTORIES "${DIRS}"
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
set(NEKO_VERSION_MINOR 1)
set(NEKO_VERSION_PATCH 0)
set(NEKO_VERSION ${NEKO_VERSION_MAJOR}.${NEKO_VERSION_MINOR}.${NEKO_VERSION_PATCH})

# Determine target endianness
TEST_BIG_ENDIAN(TARGET_BIG_ENDIAN)

set(WITH_REGEXP ON CACHE BOOL "Build Perl-compatible regex support.")
set(WITH_UI ON CACHE BOOL "Build GTK-2 UI support.")
set(WITH_SSL ON CACHE BOOL "Build SSL support.")
set(WITH_MYSQL ON CACHE BOOL "Build MySQL support.")
set(WITH_SQLITE ON CACHE BOOL "Build Sqlite support.")
set(WITH_APACHE ON CACHE BOOL "Build Apach modules.")
set(WITH_NEKOML ON CACHE BOOL "Build NekoML.")

# Process common headers in libraries
# TODO libraries should not be built from this file, but rather by traversing the tree using add_subdirectory
#add_subdirectory(libs)

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
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "CMAKE_OSX_DEPLOYMENT_TARGET" FORCE)
	endif()
endif()

configure_file (
	"${CMAKE_SOURCE_DIR}/vm/neko.h.in"
	"${CMAKE_BINARY_DIR}/neko.h"
)

set(ndll_targets
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

set(STATIC_DEPS_DOC "Dependencies that should be linked statically. Can be \"all\", \"none\", or a list of library names (e.g. \"${external_deps}\").")

if (WIN32)
	set(STATIC_DEPS "all")
else()
	set(STATIC_DEPS "none" CACHE STRING "${STATIC_DEPS_DOC}")

	option(RELOCATABLE "Set RPATH to $ORIGIN (Linux) / @executable_path (Mac)." ON)

	if (NOT APPLE)
		option(RUN_LDCONFIG "Run ldconfig after install." ON)
	endif()
endif()


# These ndll cannot be skipped.
set(core_ndll_targets
)

# Validate STATIC_DEPS
if (STATIC_DEPS STREQUAL "all")
	set(STATIC_DEPS ${external_deps} CACHE STRING "${STATIC_DEPS_DOC}" FORCE)
elseif (STATIC_DEPS STREQUAL "none")
	message(STATUS "set STATIC_DEPS to nothing")
	set(STATIC_DEPS CACHE STRING "${STATIC_DEPS_DOC}" FORCE)
endif()

foreach(dep ${STATIC_DEPS})
	list(FIND external_deps ${dep} idx)
	if(idx EQUAL -1)
		message(FATAL_ERROR "Invalid STATIC_DEPS. There is no ${dep} in the list of ${external_deps}")
	endif()
endforeach()

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


# CMAKE_BUILD_WITH_INSTALL_RPATH should be set to true.
# It is because `nekotools boot` will use the neko in build dir during build.
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

if (RELOCATABLE)
	# https://cmake.org/Wiki/CMake_RPATH_handling
	set(CMAKE_SKIP_BUILD_RPATH FALSE)
	set(CMAKE_SKIP_INSTALL_RPATH FALSE)
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
	add_compile_options(-fno-omit-frame-pointer)

	if (${CMAKE_VERSION} VERSION_LESS 2.8.10)
		add_compile_options(-fPIC)
		set(ARG_PIC -DCMAKE_C_FLAGS=-fPIC)
	else()
		set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
		set(ARG_PIC -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE)
	endif()

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
)
if(NOT ${CMAKE_VERSION} VERSION_LESS 3.1)
	list(APPEND EP_CONFIGS
		DOWNLOAD_NO_PROGRESS 1
	)
endif()
if(${CMAKE_VERSION} VERSION_LESS 3.2)
	list(APPEND EP_CONFIGS
		STEP_TARGETS download
	)
else()
	list(APPEND EP_CONFIGS
		INDEPENDENT_STEP_TARGETS download
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
					${CMAKE_SOURCE_DIR}/libs/gc/CMakeLists.txt
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC/CMakeLists.txt
				&& cd ${CMAKE_BINARY_DIR}/libs/src/BoehmGC &&
				${GIT_EXECUTABLE} apply --ignore-space-change --ignore-whitespace --whitespace=nowarn --unsafe-paths ${CMAKE_SOURCE_DIR}/libs/gc/av_safety.patch
			INSTALL_COMMAND
				${CMAKE_COMMAND} -E copy_directory
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC/include
					${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include/gc
		)
		set(GC_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include)
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
		if (APPLE)
			set(GC_CFLAGS "-w -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		else()
			set(GC_CFLAGS "-w")
		endif()

		ExternalProject_Add(libatomic_ops
			${EP_CONFIGS}
			URL http://www.ivmaisoft.com/_bin/atomic_ops/libatomic_ops-7.4.2.tar.gz
			URL_MD5 1d6538604b314d2fccdf86915e5c0857
			CONFIGURE_COMMAND echo skip config
			BUILD_COMMAND echo skip build
			INSTALL_COMMAND echo skip install
		)

		ExternalProject_Add(BoehmGC
			${EP_CONFIGS}
			DEPENDS libatomic_ops
			URL http://hboehm.info/gc/gc_source/gc-7.4.2.tar.gz
			URL_MD5 12c05fd2811d989341d8c6d81f66af87
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
		)

		# don't want to add libatomic_ops to external_deps,
		# but want download_static_deps depends on it
		add_dependencies(BoehmGC-download libatomic_ops-download)

		set(GC_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/include)
		set(GC_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/BoehmGC-build/lib/libgc.a
		)
	endif()
	set_target_properties(BoehmGC PROPERTIES ${EP_PROPS})
	add_dependencies(libneko BoehmGC)
else()
	find_package(BoehmGC REQUIRED)
endif()

target_include_directories(libneko PRIVATE ${GC_INCLUDE_DIR})

target_link_libraries(libneko ${GC_LIBRARIES})
target_link_libraries(nekovm libneko)

if(UNIX)
        if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
		set(DL_LIB "dl")
	endif()
	find_package(Threads)
	target_link_libraries(libneko ${DL_LIB} m ${CMAKE_THREAD_LIBS_INIT})
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
)

#######################

# compilers
# nekoc, nekoml, nekotools, and test.n

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

file(GLOB compilers_src
	src/neko/*.nml
	src/nekoml/*.nml
	boot/*.n
)

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n
	COMMAND ${set_neko_env}

	COMMAND ${neko_exec} ../boot/nekoml.n -nostd neko/Main.nml nekoml/Main.nml
	COMMAND ${neko_exec} ../boot/nekoc.n -link ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoc.n neko/Main
	COMMAND ${neko_exec} ../boot/nekoc.n -link ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.n nekoml/Main

	VERBATIM
	DEPENDS nekovm std.ndll ${compilers_src}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

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

add_custom_target(download_static_deps)
if (STATIC_BOEHMGC)
	add_dependencies(download_static_deps BoehmGC-download)
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
	DEPENDS source_archive download_static_deps
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
	set(DEST_INCLUDE ${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_LIBRARY_ARCHITECTURE})
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
	PUBLIC_HEADER DESTINATION ${DEST_INCLUDE}
)

if (WITH_NEKOML)
	install (
		TARGETS
			nekoml
		EXPORT NekoTargets
		RUNTIME DESTINATION ${DEST_BIN}
		LIBRARY DESTINATION ${DEST_LIB}
		PUBLIC_HEADER DESTINATION ${DEST_INCLUDE}
	)
	install (
		FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nekoml.std
		DESTINATION ${DEST_NDLL}
	)
endif()

if (NOT ${CMAKE_VERSION} VERSION_LESS 2.8.8)
	include(CMakePackageConfigHelpers)

	if(WIN32 AND NOT CYGWIN)
		set(DEF_INSTALL_CMAKE_DIR CMake)
	else()
		set(DEF_INSTALL_CMAKE_DIR lib/cmake/Neko)
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
endif()

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
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set(OS_NAME "freebsd")
else()
	message( WARNING "unknown ${CMAKE_SYSTEM_NAME}" )
	set(OS_NAME "")
endif()

set(bin_archive_name_we "neko-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${OS_NAME}${arch_64}")
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
	
	set_tests_properties(nekoml
		PROPERTIES
		ENVIRONMENT LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)
endif()

add_test(NAME nekotools
	COMMAND nekotools
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

if (UNIX AND NOT APPLE)
	set_tests_properties(-version test.n nekoc nekotools
		PROPERTIES
		ENVIRONMENT LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)
endif()


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
