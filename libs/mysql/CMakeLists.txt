
######################
# OpenSSL

if (STATIC_OPENSSL)
	if (WIN32)
		# perl is needed to run the openssl Configure script...
		find_package(Perl REQUIRED)
		if (arch_64)
			set(openssl_target VC-WIN64A)
		else()
			set(openssl_target VC-WIN32)
		endif()

		set(OPENSSL_CONFIGURE_COMMAND ${PERL_EXECUTABLE} Configure ${openssl_target} no-asm)
		set(OPENSSL_MAKE_COMMAND nmake /S)
	else()
		if (APPLE)
			if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
				set(OPENSSL_CONFIGURE_COMMAND ./Configure darwin-i386-cc "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
			elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
				set(OPENSSL_CONFIGURE_COMMAND ./Configure darwin64-x86_64-cc "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
			elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
				set(OPENSSL_CONFIGURE_COMMAND ./Configure darwin64-arm64-cc "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
			endif()
		else()
			set(OPENSSL_CONFIGURE_COMMAND ./config)
		endif()

		set(OPENSSL_MAKE_COMMAND make)
	endif()
	set(OPENSSL_CONFIGS
		CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
			${OPENSSL_CONFIGURE_COMMAND} no-tests no-shared --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix --libdir=lib
		BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
			${OPENSSL_MAKE_COMMAND}
		INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
			${OPENSSL_MAKE_COMMAND} install_sw
	)
	if (WIN32)
		set(OPENSSL_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libssl.lib
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libcrypto.lib
		)
	else()
		set(OPENSSL_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libssl.a
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libcrypto.a
		)
	endif()
	ExternalProject_Add(OpenSSL
		${EP_CONFIGS}
		URL https://www.openssl.org/source/openssl-3.0.8.tar.gz
		URL_HASH SHA256=6c13d2bf38fdf31eac3ce2a347073673f5d63263398f1f69d0df4a41253e4b3e
		${OPENSSL_CONFIGS}
		BYPRODUCTS ${OPENSSL_LIBRARIES}
		SOURCE_DIR ${CMAKE_BINARY_DIR}/libs/src/openssl
	)
	set_target_properties(OpenSSL PROPERTIES ${EP_PROPS})
	# Download project for fat source archive
	add_dependencies(download_deps OpenSSL-download)
endif()

######################
# mysql.ndll

add_library(mysql.ndll MODULE mysql.c)

if (STATIC_MARIADBCONNECTOR)
	if (STATIC_OPENSSL)
		set(OPENSSL_CONF -DOPENSSL_USE_STATIC_LIBS=ON -DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(OPENSSL_DEP OpenSSL)
	elseif()
		set(OPENSSL_CONF "")
		set(OPENSSL_DEP "")
	endif()
	if (STATIC_ZLIB)
		set(ZLIB_CONF
			-DWITH_EXTERNAL_ZLIB=ON
			-DZLIB_ROOT=${CMAKE_BINARY_DIR}/libs/src/install-prefix
		)
		set(ZLIB_DEP Zlib)
	else()
		if (UNIX)
			set(ZLIB_CONF -DWITH_EXTERNAL_ZLIB=ON)
		endif()
		set(ZLIB_DEP "")
	endif()
	if (WIN32)
		set(MARIADB_CONNECTOR_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/${CMAKE_CFG_INTDIR}/mariadbclient.lib
		)
	else()
		set(MARIADB_CONNECTOR_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/libmariadbclient.a
		)
	endif()
	ExternalProject_Add(MariaDBConnector
		${EP_CONFIGS}
		DEPENDS ${OPENSSL_DEP} ${ZLIB_DEP}
		URL https://downloads.mariadb.com/Connectors/c/connector-c-3.3.4/mariadb-connector-c-3.3.4-src.tar.gz
		URL_HASH SHA256=486e5fdf976a8e7fadf583ae912128655e013ac575fa79b2d1af0fb8827a78ed
		CMAKE_ARGS
			-Wno-dev
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
			-DWITH_SSL=OPENSSL
			${OPENSSL_CONF}
			${ZLIB_CONF}
		PATCH_COMMAND ${CMAKE_COMMAND} -Dmariadb_source=${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector -P ${CMAKE_SOURCE_DIR}/cmake/patch_mariadb.cmake
		BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build &&
			${CMAKE_COMMAND} --build . --target mariadbclient --config ${CMAKE_CFG_INTDIR}
		INSTALL_COMMAND echo skip install
		BUILD_BYPRODUCTS ${MARIADB_CONNECTOR_LIBRARIES}
	)
	set_target_properties(MariaDBConnector PROPERTIES ${EP_PROPS})
	set(MARIADB_CONNECTOR_INCLUDE_DIR
		${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector/include
		${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/include
	)
	add_dependencies(mysql.ndll MariaDBConnector)
	# Download project for fat source archive
	add_dependencies(download_deps MariaDBConnector-download)
else()
	find_package(MariaDBConnector REQUIRED)
endif()

target_include_directories(mysql.ndll
	PRIVATE
	${MARIADB_CONNECTOR_INCLUDE_DIR}
)

target_link_libraries(mysql.ndll libneko ${MARIADB_CONNECTOR_LIBRARIES} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES})

if (WIN32)
	target_link_libraries(mysql.ndll ws2_32 crypt32 shlwapi secur32)
endif()

set_target_properties(mysql.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mysql
	SUFFIX .ndll
)

if(APPLE)
	set_target_properties(mysql.ndll
		PROPERTIES
		LINK_FLAGS "-undefined dynamic_lookup ${LINK_FLAGS}"
	)
endif()

######################
# mysql5.ndll

add_library(mysql5.ndll MODULE
	my_proto/my_proto.c
	my_proto/my_api.c
	mysql.c
)

target_include_directories(mysql5.ndll
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/my_proto
)

target_link_libraries(mysql5.ndll
	socket
	sha1
	libneko
)

if (WIN32)
	target_link_libraries(mysql5.ndll ws2_32)
endif()

set_target_properties(mysql5.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mysql5
	SUFFIX .ndll
)

install (
	TARGETS mysql.ndll mysql5.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
