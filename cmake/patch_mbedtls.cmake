if (WIN32)
	file(COPY ${source}/libs/ssl/threading_alt.h
		DESTINATION ${MbedTLS_source}/include/mbedtls/
	)
endif()
