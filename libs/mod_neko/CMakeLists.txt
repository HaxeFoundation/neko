
######################
# mod_neko2.ndll

add_library(mod_neko2.ndll MODULE
	../../vm/stats.c # FIXME
	mod_neko.c
	cgi.c
)

target_include_directories(mod_neko2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIRS}
)

target_link_libraries(mod_neko2.ndll libneko ${APACHE_LIBRARIES})

# In static Apache case build dependencies first
if (STATIC_APACHE)
	add_dependencies(mod_neko2.ndll Apache APR APRutil)
endif()

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

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
