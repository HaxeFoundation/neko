get_filename_component(source_archive_dir ${source_archive} DIRECTORY)

execute_process(
	COMMAND ${CMAKE_COMMAND} -E tar x ${source_archive}
	WORKING_DIRECTORY ${source_archive_dir}
)

file(RENAME ${source_archive_dir}/${source_archive_name_we} ${source_archive_dir}/${source_archive_fat_name_we})


if (${CMAKE_VERSION} VERSION_LESS 3.3)
	file(GLOB archives
		${bin_dir}/${lib_src_dir}/*
	)
	foreach(file ${archives})
		if(NOT IS_DIRECTORY ${file})
			file(COPY ${file} DESTINATION ${source_archive_dir}/${source_archive_fat_name_we}/${lib_src_dir})
		endif()
	endforeach()
else()
	file(GLOB archives
		LIST_DIRECTORIES FALSE
		${bin_dir}/${lib_src_dir}/*
	)
	file(COPY ${archives} DESTINATION ${source_archive_dir}/${source_archive_fat_name_we}/${lib_src_dir})
endif()

if (${source_archive_fat_name} MATCHES ^.*.zip$)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar cf ${source_archive_fat_name} ${source_archive_dir}/${source_archive_fat_name_we} --format=zip
		WORKING_DIRECTORY ${source_archive_dir}
	)
else()
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar czf ${source_archive_fat_name} ${source_archive_dir}/${source_archive_fat_name_we} 
		WORKING_DIRECTORY ${source_archive_dir}
	)
endif()

file(REMOVE_RECURSE ${source_archive_dir}/${source_archive_fat_name_we})
