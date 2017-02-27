configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/osdef.h.in"
	"${CMAKE_BINARY_DIR}/osdef.h"
)

add_library(socket socket.c)
add_library(sha1 sha1.c)
