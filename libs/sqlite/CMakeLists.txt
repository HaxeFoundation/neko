
######################
# sqlite.ndll

if (STATIC_SQLITE3)
	ExternalProject_Add(Sqlite3
		${EP_CONFIGS}
		URL https://www.sqlite.org/2018/sqlite-autoconf-3250300.tar.gz
		URL_MD5 06db8206bc8febf07141b78ad58595ea
		CONFIGURE_COMMAND echo skip config
		BUILD_COMMAND echo skip build
		INSTALL_COMMAND echo skip install
	)
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/libs/src/Sqlite3/sqlite3.c
		DEPENDS Sqlite3
	)

	add_library(sqlite.ndll MODULE
		sqlite.c
		${CMAKE_BINARY_DIR}/libs/src/Sqlite3/sqlite3.c
	)
	target_include_directories(sqlite.ndll PRIVATE ${CMAKE_BINARY_DIR}/libs/src/Sqlite3)
	target_link_libraries(sqlite.ndll libneko)
	target_compile_definitions(sqlite.ndll PRIVATE SQLITE_MAX_VARIABLE_NUMBER=250000 SQLITE_ENABLE_RTREE=1)
	# Download project for fat source archive
	add_dependencies(download_static_deps Sqlite3-download)
else()
	add_library(sqlite.ndll MODULE sqlite.c)
	pkg_check_modules(SQLITE3 REQUIRED sqlite3)
	target_include_directories(sqlite.ndll PRIVATE ${SQLITE3_INCLUDEDIR} ${SQLITE3_INCLUDE_DIRS})
	target_link_libraries(sqlite.ndll libneko ${SQLITE3_LIBRARIES})
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
