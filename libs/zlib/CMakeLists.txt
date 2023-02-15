
######################
# zlib.ndll

add_library(zlib.ndll MODULE zlib.c)

if(STATIC_ZLIB)
	add_dependencies(zlib.ndll Zlib)
endif()

target_include_directories(zlib.ndll PRIVATE ${ZLIB_INCLUDE_DIRS})
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
