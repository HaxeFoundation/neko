include(FindPkgConfig)

######################
# ui.ndll

add_library(ui.ndll MODULE ui.c)

target_link_libraries(ui.ndll libneko)

if(APPLE)
	find_library(CARBON_LIBRARY Carbon REQUIRED)
	target_link_libraries(ui.ndll ${CARBON_LIBRARY})
elseif(UNIX)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	target_include_directories(ui.ndll PRIVATE
		${GTK3_INCLUDEDIR}
		${GTK3_INCLUDE_DIRS}
	)
	target_link_libraries(ui.ndll ${GTK3_LIBRARIES})
endif()

set_target_properties(ui.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME ui
	SUFFIX .ndll
)

install (
	TARGETS ui.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
