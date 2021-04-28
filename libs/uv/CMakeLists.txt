include(FindPkgConfig)

######################
# uv.ndll

add_library(uv.ndll MODULE uv.c)

target_link_libraries(uv.ndll uv libneko)

set_target_properties(uv.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME uv
	SUFFIX .ndll
)

install (
	TARGETS uv.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
