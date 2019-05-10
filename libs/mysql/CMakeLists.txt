
######################
# OpenSSL

if (STATIC_OPENSSL)
	if (APPLE)
		if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "i386")
			set(OPENSSL_CONF ./Configure darwin-i386-cc "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		elseif (${CMAKE_OSX_ARCHITECTURES} STREQUAL "x86_64")
			set(OPENSSL_CONF ./Configure darwin64-x86_64-cc "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		endif()
	else()
		set(OPENSSL_CONF ./config)
	endif()
	if (WIN32)
		# perl is needed to run the openssl Configure script...
		find_package(Perl REQUIRED)
		if (arch_64)
			set(openssl_target VC-WIN64A)
		else()
			set(openssl_target VC-WIN32)
		endif()
		message("arch_64 is ${arch_64}")
		message("openssl_target is ${openssl_target}")
		set(OPENSSL_CONFS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
				${PERL_EXECUTABLE} Configure ${openssl_target} no-asm no-tests --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
				nmake /S
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
				nmake /S install_sw
		)
	else()
		set(OPENSSL_CONFS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
				${OPENSSL_CONF} no-tests --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
				make
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/openssl &&
				make install_sw
		)
	endif()
	ExternalProject_Add(OpenSSL
		${EP_CONFIGS}
		URL https://www.openssl.org/source/openssl-1.1.1.tar.gz
		URL_MD5 7079eb017429e0ffb9efb42bf80ccb21
		${OPENSSL_CONFS}
		PATCH_COMMAND ${CMAKE_COMMAND} -Dopenssl_source=${CMAKE_BINARY_DIR}/libs/src/openssl -P ${CMAKE_SOURCE_DIR}/cmake/patch_openssl.cmake
		SOURCE_DIR ${CMAKE_BINARY_DIR}/libs/src/openssl
	)
	set_target_properties(OpenSSL PROPERTIES ${EP_PROPS})
	if (WIN32)
		set(OPENSSL_LIBRARIES
			Crypt32
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libcrypto.lib
			${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libssl.lib
		)
	endif()
	# Download project for fat source archive
	add_dependencies(download_static_deps OpenSSL-download)
else()
	find_package(OpenSSL)
endif()

######################
# mysql.ndll

add_library(mysql.ndll MODULE mysql.c)

if (STATIC_MARIADBCONNECTOR)
	if (STATIC_OPENSSL AND NOT WIN32)
		set(OPENSSL_CONF -DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
		set(OPENSSL_DEP OpenSSL)
	elseif()
		set(OPENSSL_CONF "")
		set(OPENSSL_DEP "")
	endif()
	if (WIN32)
		set(MARIADB_CONNECTOR_LIBRARIES
			${OPENSSL_LIBRARIES}
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/${CMAKE_CFG_INTDIR}/mariadbclient.lib
		)
	else()
		set(MARIADB_CONNECTOR_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/libmariadbclient.a
		)
	endif()
	ExternalProject_Add(MariaDBConnector
		${EP_CONFIGS}
		DEPENDS ${OPENSSL_DEP}
		URL https://downloads.mariadb.org/f/connector-c-3.0.9/mariadb-connector-c-3.0.9-src.tar.gz
		URL_MD5 e7fdb17e26e779f4bd15b6a32e1c8472
		CMAKE_ARGS
			-Wno-dev
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
			-DWITH_SSL=ON
			${OPENSSL_CONF}
		PATCH_COMMAND ${CMAKE_COMMAND} -Dmariadb_source=${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector -P ${CMAKE_SOURCE_DIR}/cmake/patch_mariadb.cmake
		BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build &&
			${CMAKE_COMMAND} --build . --target mariadbclient --config ${CMAKE_CFG_INTDIR}
		INSTALL_COMMAND echo skip install
		BYPRODUCTS ${MARIADB_CONNECTOR_LIBRARIES}
	)
	set_target_properties(MariaDBConnector PROPERTIES ${EP_PROPS})
	set(MARIADB_CONNECTOR_INCLUDE_DIR
		${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector/include
		${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/include
	)
	add_dependencies(mysql.ndll MariaDBConnector)
	# Download project for fat source archive
	add_dependencies(download_static_deps MariaDBConnector-download)
else()
	find_package(MariaDBConnector REQUIRED)
endif()

target_include_directories(mysql.ndll
	PRIVATE
	${MARIADB_CONNECTOR_INCLUDE_DIR}
)

target_link_libraries(mysql.ndll libneko ${MARIADB_CONNECTOR_LIBRARIES})

if (WIN32)
	target_link_libraries(mysql.ndll ws2_32 crypt32 shlwapi)
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
endif(APPLE)

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
