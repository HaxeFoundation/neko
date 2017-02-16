
######################
# regexp.ndll

add_library(regexp.ndll MODULE regexp.c)
if (STATIC_PCRE)
	if (WIN32)
		ExternalProject_Add(PCRE
			${EP_CONFIGS}
			URL http://downloads.sourceforge.net/project/pcre/pcre/8.39/pcre-8.39.tar.gz
			URL_MD5 26a76d97e04c89fe9ce22ecc1cd0b315
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
			URL http://downloads.sourceforge.net/project/pcre/pcre/8.39/pcre-8.39.tar.gz
			URL_MD5 26a76d97e04c89fe9ce22ecc1cd0b315
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
	# Download project for fat source archive
	add_dependencies(download_static_deps PCRE-download)
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

install (
	TARGETS regexp.ndll
	DESTINATION ${DEST_NDLL}
)
