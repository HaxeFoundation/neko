
######################
# mod_neko2.ndll

add_library(mod_neko2.ndll MODULE ${EXCLUDE_MOD_NEKO2_NDLL_FROM_ALL}
	../../vm/stats.c # FIXME
	mod_neko.c
	cgi.c
)


target_include_directories(mod_neko2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIRS}
)

target_link_libraries(mod_neko2.ndll libneko ${APACHE_LIBRARIES})

set_target_properties(mod_neko2.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mod_neko2
	SUFFIX .ndll
)

if(APPLE)
	set_target_properties(mod_neko2.ndll
		PROPERTIES
		LINK_FLAGS "-undefined dynamic_lookup ${LINK_FLAGS}"
	)
endif(APPLE)

install (
	TARGETS mod_neko2.ndll
	DESTINATION ${DEST_NDLL}
)
