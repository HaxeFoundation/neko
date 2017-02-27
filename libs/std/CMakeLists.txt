
add_library(std.ndll MODULE
	buffer.c
	date.c
	file.c
	init.c
	int32.c
	math.c
	string.c
	random.c
	serialize.c
	socket.c
	sys.c
	xml.c
	module.c
	md5.c
	unicode.c
	utf8.c
	memory.c
	misc.c
	thread.c
	process.c
	elf_update.c
)

target_link_libraries(std.ndll
	sha1
	libneko
	)

if(WIN32)
	target_link_libraries(std.ndll ws2_32)
endif()

set_target_properties(std.ndll
	PROPERTIES
	PREFIX ""
	OUTPUT_NAME std
	SUFFIX .ndll
)

install (
	TARGETS std.ndll
	DESTINATION ${DEST_NDLL}
)

install(SCRIPT ${NEKO_FLATTEN_SCRIPT})
