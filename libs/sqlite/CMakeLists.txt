
######################
# sqlite.ndll

if (STATIC_SQLITE3)
	ExternalProject_Add(SQLite3
		${EP_CONFIGS}
		URL https://www.sqlite.org/2022/sqlite-autoconf-3400100.tar.gz
		URL_HASH SHA256=2c5dea207fa508d765af1ef620b637dcb06572afa6f01f0815bd5bbf864b33d9
		CONFIGURE_COMMAND echo skip config
		BUILD_COMMAND echo skip build
		INSTALL_COMMAND echo skip install
	)
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/libs/src/SQLite3/sqlite3.c
		DEPENDS SQLite3
	)

	add_library(sqlite.ndll MODULE
		sqlite.c
		${CMAKE_BINARY_DIR}/libs/src/SQLite3/sqlite3.c
	)
	target_include_directories(sqlite.ndll PRIVATE ${CMAKE_BINARY_DIR}/libs/src/SQLite3)
	target_link_libraries(sqlite.ndll libneko)
	target_compile_definitions(sqlite.ndll PRIVATE SQLITE_MAX_VARIABLE_NUMBER=250000 SQLITE_ENABLE_RTREE=1)
	# Download project for fat source archive
	add_dependencies(download_deps SQLite3-download)
else()
	add_library(sqlite.ndll MODULE sqlite.c)
	find_package(SQLite3 REQUIRED)
	target_include_directories(sqlite.ndll PRIVATE ${SQLite3_INCLUDE_DIRS})
	target_link_libraries(sqlite.ndll libneko ${SQLite3_LIBRARIES})
endif()


set_target_properties(sqlite.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME sqlite
	SUFFIX .ndll
)

install (
	TARGETS sqlite.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
