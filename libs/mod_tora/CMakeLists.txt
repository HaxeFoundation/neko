
######################
# mod_tora2.ndll

add_library(mod_tora2.ndll MODULE
	protocol.c
	mod_tora.c
)

add_dependencies(mod_tora2.ndll
	mod_neko2.ndll
	socket
	)

target_include_directories(mod_tora2.ndll
	PRIVATE
	${APACHE_INCLUDE_DIRS}
)

target_link_libraries(mod_tora2.ndll
	socket
	${APACHE_LIBRARIES}
	)

if (WIN32)
	target_link_libraries(mod_tora2.ndll ws2_32)
endif()

set_target_properties(mod_tora2.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME mod_tora2
	SUFFIX .ndll
)

#######################

if(APPLE)
	set_target_properties(mod_tora2.ndll
		PROPERTIES
		LINK_FLAGS "-undefined dynamic_lookup ${LINK_FLAGS}"
	)
endif(APPLE)

install (
	TARGETS mod_tora2.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
