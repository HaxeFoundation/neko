configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/osdef.h.in"
	"${CMAKE_BINARY_DIR}/osdef.h"
)

add_library(socket STATIC socket.c)
add_library(sha1 STATIC sha1.c)
