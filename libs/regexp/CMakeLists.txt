
######################
# regexp.ndll

add_library(regexp.ndll MODULE regexp.c)
if (STATIC_PCRE2)
	add_compile_definitions(PCRE2_STATIC_LINK)
	set(PCRE2_CONFIG
		URL
			"https://github.com/PhilipHazel/pcre2/releases/download/pcre2-10.37/pcre2-10.37.tar.gz"
			"https://sourceforge.net/projects/pcre/files/pcre2/10.37/pcre2-10.37.tar.gz/download"
		URL_MD5 a0b59d89828f62d2e1caac04f7c51e0b
	)
	if (WIN32)
		ExternalProject_Add(pcre2
			${EP_CONFIGS}
			${PCRE2_CONFIG}
			CMAKE_ARGS
				-G ${CMAKE_GENERATOR}
				-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
				-Wno-dev
				-DPCRE2_BUILD_PCRE2GREP=OFF
				-DPCRE2_BUILD_TESTS=OFF
				-DPCRE2_SUPPORT_JIT=ON
				-DPCRE2_SUPPORT_UNICODE=ON
		)
		set(PCRE2_LIBRARIES
			optimized ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/pcre2-8.lib
			debug ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/pcre2-8d.lib
		)
	else()
		if (APPLE)
			set(PCRE2_CFLAGS "-w -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		else()
			set(PCRE2_CFLAGS "-w")
		endif()
		set(PCRE2_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libpcre2-8.a
		)
		ExternalProject_Add(pcre2
			${EP_CONFIGS}
			${PCRE2_CONFIG}
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/pcre2 &&
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
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/pcre2 &&
				make "CFLAGS=${PCRE_CFLAGS}"
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/pcre2 &&
				make install
			BYPRODUCTS ${PCRE2_LIBRARIES}
		)
	endif()
	set_target_properties(pcre2 PROPERTIES ${EP_PROPS})
	set(PCRE2_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/install-prefix/include)
	add_dependencies(regexp.ndll pcre2)
	# Download project for fat source archive
	add_dependencies(download_static_deps pcre2-download)
else()
	find_package(PCRE2 REQUIRED)
endif()

target_include_directories(regexp.ndll PRIVATE ${PCRE2_INCLUDE_DIRS})
target_link_libraries(regexp.ndll libneko ${PCRE2_LIBRARIES})

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
