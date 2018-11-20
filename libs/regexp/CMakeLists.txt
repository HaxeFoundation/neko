
######################
# regexp.ndll

add_library(regexp.ndll MODULE regexp.c)
if (STATIC_PCRE)
	if (WIN32)
		set(PCRE_URL "https://ftp.pcre.org/pub/pcre/pcre-8.42.tar.gz")
		if (NOT ${CMAKE_VERSION} VERSION_LESS 3.7)
			list(APPEND PCRE_URL
				"http://downloads.sourceforge.net/project/pcre/pcre/8.42/pcre-8.42.tar.gz"
			)
		endif()
		ExternalProject_Add(PCRE
			${EP_CONFIGS}
			URL ${PCRE_URL}
			URL_MD5 fc18afa0f14a25475cf097ee102a3e4f
			CMAKE_ARGS
				-G ${CMAKE_GENERATOR}
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
		if (APPLE)
			set(PCRE_CFLAGS "-w -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		else()
			set(PCRE_CFLAGS "-w")
		endif()
		set(PCRE_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libpcre.a
		)
		set(PCRE_URL "https://ftp.pcre.org/pub/pcre/pcre-8.40.tar.gz")
		if (NOT ${CMAKE_VERSION} VERSION_LESS 3.7)
			list(APPEND PCRE_URL
				"http://downloads.sourceforge.net/project/pcre/pcre/8.40/pcre-8.40.tar.gz"
			)
		endif()
		ExternalProject_Add(PCRE
			${EP_CONFIGS}
			URL ${PCRE_URL}
			URL_MD5 890c808122bd90f398e6bc40ec862102
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
				make "CFLAGS=${PCRE_CFLAGS}"
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/PCRE &&
				make install
			BYPRODUCTS ${PCRE_LIBRARIES}
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

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
