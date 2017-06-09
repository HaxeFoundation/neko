include(FindPkgConfig)

######################
# zlib.ndll

add_library(zlib.ndll MODULE zlib.c)


if (STATIC_ZLIB)
	set(ZLIB_CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
		-Wno-dev
	)
	if (UNIX)
		list(APPEND ZLIB_CMAKE_ARGS
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
			${ARG_PIC}
		)
	endif()
	if (WIN32)
		set(ZLIB_LIBRARIES
			optimized ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/zlibstatic.lib
			debug ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/zlibstaticd.lib
		)
	else()
		set(ZLIB_LIBRARIES ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libz.a)
	endif()

	# Get the current config. Borrowed from
	# https://github.com/Kitware/CMake/blob/bc7d64f896d6e180970cb404cc7699732db34adc/Modules/ExternalProject.cmake
	if (CMAKE_CFG_INTDIR AND
		NOT CMAKE_CFG_INTDIR STREQUAL "." AND
		NOT CMAKE_CFG_INTDIR MATCHES "\\$")
		set(config ${CMAKE_CFG_INTDIR})
	else()
		set(config $<CONFIG>)
	endif()

	ExternalProject_Add(Zlib
		${EP_CONFIGS}
		URL http://zlib.net/zlib-1.2.11.tar.gz
		URL_MD5 1c9f62f0778697a09d36121ead88e08e
		CMAKE_ARGS ${ZLIB_CMAKE_ARGS}
		INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/Zlib-build &&
			${CMAKE_COMMAND} --build . --target install --config ${config}
		BYPRODUCTS ${ZLIB_LIBRARIES}
	)
	set_target_properties(Zlib PROPERTIES ${EP_PROPS})
	set(ZLIB_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/install-prefix/include)
	add_dependencies(zlib.ndll Zlib)
	target_include_directories(zlib.ndll PRIVATE ${ZLIB_INCLUDE_DIRS})
	# Download project for fat source archive
	add_dependencies(download_static_deps Zlib-download)
else()
	pkg_check_modules(ZLIB REQUIRED zlib)
	target_include_directories(zlib.ndll PRIVATE ${ZLIB_INCLUDEDIR} ${ZLIB_INCLUDE_DIRS})
endif()

target_link_libraries(zlib.ndll libneko ${ZLIB_LIBRARIES})

set_target_properties(zlib.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME zlib
	SUFFIX .ndll
)

install (
	TARGETS zlib.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
