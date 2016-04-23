# Apply config adjustments similer to Debian's
# https://anonscm.debian.org/cgit/collab-maint/mbedtls.git/tree/debian/patches/01_config.patch

set(config ${MbedTLS_source}/include/mbedtls/config.h)

file(READ ${config} content)

# disable support for SSL 3.0
string(REPLACE 
	"#define MBEDTLS_SSL_PROTO_SSL3"
	"//#define MBEDTLS_SSL_PROTO_SSL3"
	content ${content}
)

if (WIN32)
	# allow alternate threading implementation
	string(REPLACE 
		"//#define MBEDTLS_THREADING_ALT"
		"#define MBEDTLS_THREADING_ALT"
		content ${content}
	)
	# disable the TCP/IP networking routines
	# such that it wouldn't interfere with the #include <windows.h> in our threading_alt.h
	string(REPLACE 
		"#define MBEDTLS_NET_C"
		"//#define MBEDTLS_NET_C"
		content ${content}
	)

	file(COPY ${source}/libs/ssl/threading_alt.h
		DESTINATION ${MbedTLS_source}/include/mbedtls/
	)
else()
	# enable pthread mutexes
	string(REPLACE 
		"//#define MBEDTLS_THREADING_PTHREAD"
		"#define MBEDTLS_THREADING_PTHREAD"
		content ${content}
	)
endif()

# enable the HAVEGE random generator
string(REPLACE 
	"//#define MBEDTLS_HAVEGE_C"
	"#define MBEDTLS_HAVEGE_C"
	content ${content}
)
# enable support for (rare) MD2-signed X.509 certs
string(REPLACE 
	"//#define MBEDTLS_MD2_C"
	"#define MBEDTLS_MD2_C"
	content ${content}
)
# enable support for (rare) MD4-signed X.509 certs
string(REPLACE 
	"//#define MBEDTLS_MD4_C"
	"#define MBEDTLS_MD4_C"
	content ${content}
)
# allow use of mutexes within mbed TLS
string(REPLACE 
	"//#define MBEDTLS_THREADING_C"
	"#define MBEDTLS_THREADING_C"
	content ${content}
)

file(WRITE ${config} ${content})