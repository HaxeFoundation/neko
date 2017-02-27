
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
		if (arch_64)
			set(openssl_target VC-WIN64A)
			set(openssl_script ms/do_win64a.bat)
		else()
			set(openssl_target VC-WIN32)
			set(openssl_script ms/do_ms.bat)
		endif()
		message("arch_64 is ${arch_64}")
		message("openssl_target is ${openssl_target}")
		message("openssl_script is ${openssl_script}")
		set(OPENSSL_CONFS
			CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				${PERL_EXECUTABLE} Configure ${openssl_target} no-asm --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix &&
				call ${openssl_script}
			BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				nmake /S -f ms/nt.mak PLATFORM=${openssl_target}
			INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/OpenSSL &&
				nmake /S -f ms/nt.mak PLATFORM=${openssl_target} install
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
		URL https://www.openssl.org/source/openssl-1.0.2k.tar.gz
		URL_MD5 f965fc0bf01bf882b31314b61391ae65
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
	# Download project for fat source archive
	add_dependencies(download_static_deps OpenSSL-download)
else()
	find_package(OpenSSL)
endif()

######################
# mysql.ndll

add_library(mysql.ndll MODULE mysql.c)

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
		URL https://downloads.mariadb.org/f/connector-c-2.3.2/mariadb-connector-c-2.3.2-src.tar.gz
		URL_MD5 cbee903e8581ca73b0aecfe9b7498c65
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
			${OPENSSL_LIBRARIES}
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/${CMAKE_CFG_INTDIR}/mariadbclient.lib
		)
	else()
		set(MARIADB_CONNECTOR_LIBRARIES
			${CMAKE_BINARY_DIR}/libs/src/MariaDBConnector-build/libmariadb/libmariadbclient.a
		)
	endif()
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
