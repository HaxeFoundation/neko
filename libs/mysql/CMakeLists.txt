
######################
# mysql.ndll

add_library(mysql.ndll MODULE ${EXCLUDE_MYSQL_NDLL_FROM_ALL} mysql.c)

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
		URL https://downloads.mariadb.org/f/connector-c-2.3.1/mariadb-connector-c-2.3.1-src.tar.gz
		URL_MD5 26a491253bd93a3831cdfc077a83ccc7
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

add_library(mysql5.ndll MODULE ${EXCLUDE_MYSQL5_NDLL_FROM_ALL}
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
