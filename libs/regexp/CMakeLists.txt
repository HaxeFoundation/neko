
######################
# regexp.ndll

add_library(regexp.ndll MODULE regexp.c)
if (STATIC_PCRE2)
	set_target_properties(regexp.ndll
		PROPERTIES
		COMPILE_DEFINITIONS PCRE2_STATIC_LINK
	)
	set(PCRE2_CONFIG
		URL https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.42/pcre2-10.42.tar.gz
		URL_HASH SHA256=c33b418e3b936ee3153de2c61cc638e7e4fe3156022a5c77d0711bcbb9d64f1f
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
				-DBUILD_SHARED_LIBS=OFF
		)
		set(PCRE2_LIBRARIES
			optimized ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/pcre2-8-static.lib
			debug ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/pcre2-8-staticd.lib
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
				autoreconf -f -i &&
				./configure
					--prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					--with-pic
					--enable-silent-rules
					--enable-jit
					--enable-shared=no
					--enable-static=yes
					--silent
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/pcre2 &&
				make "CFLAGS=${PCRE2_CFLAGS}"
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/pcre2 &&
				make install
			BYPRODUCTS ${PCRE2_LIBRARIES}
		)
	endif()
	set_target_properties(pcre2 PROPERTIES ${EP_PROPS})
	set(PCRE2_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/install-prefix/include)
	add_dependencies(regexp.ndll pcre2)
	# Download project for fat source archive
	add_dependencies(download_deps pcre2-download)
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
