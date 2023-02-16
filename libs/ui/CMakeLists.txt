
######################
# ui.ndll

add_library(ui.ndll MODULE ui.c)

target_link_libraries(ui.ndll libneko)

if(APPLE)
	find_library(CARBON_LIBRARY Carbon REQUIRED)
	target_link_libraries(ui.ndll ${CARBON_LIBRARY})
elseif(UNIX)
	find_package(GTK2 REQUIRED)
	target_include_directories(ui.ndll PRIVATE
		${GTK2_INCLUDE_DIRS}
	)
	target_link_libraries(ui.ndll ${GTK2_LIBRARIES})
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
