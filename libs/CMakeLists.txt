add_subdirectory(common)
add_subdirectory(std)
add_subdirectory(zlib)
if (WITH_MYSQL)
	add_subdirectory(mysql)
endif()
if (WITH_REGEXP)
	add_subdirectory(regexp)
endif()
if (WITH_SQLITE)
	add_subdirectory(sqlite)
endif()
if (WITH_SSL)
	add_subdirectory(ssl)
endif()
if (WITH_UI)
	add_subdirectory(ui)
endif()

if (WITH_APACHE)
	# Locate Apache
	if (STATIC_APACHE)
		if (STATIC_OPENSSL)
			set(OPENSSL_CONF --with-openssl=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
			set(OPENSSL_DEP OpenSSL)
		elseif()
			set(OPENSSL_CONF "")
			set(OPENSSL_DEP "")
		endif()
		if (STATIC_APR)
			set(APR_CONF --with-apr=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
			set(APR_DEP APR)
		elseif()
			set(APR_CONF "")
			set(APR_DEP "")
		endif()
		if (STATIC_APRUTIL)
			set(APRUTIL_CONF --with-apr-util=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
			set(APRUTIL_DEP APRutil)
		elseif()
			set(APRUTIL_CONF "")
			set(APRUTIL_DEP "")
		endif()
		if (STATIC_PCRE)
			set(PCRE_CONF --with-pcre=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
			set(PCRE_DEP PCRE)
		elseif()
			set(PCRE_CONF "")
			set(PCRE_DEP "")
		endif()
		if (STATIC_ZLIB)
			set(ZLIB_CONF --with-z=${CMAKE_BINARY_DIR}/libs/src/install-prefix)
			set(ZLIB_DEP Zlib)
		elseif()
			set(ZLIB_CONF "")
			set(ZLIB_DEP "")
		endif()

		if (APPLE)
			set(APACHE_CFLAGS "-w -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		else()
			set(APACHE_CFLAGS "-w")
		endif()

		if(WIN32)
			set(APR_CONFIGS
				CMAKE_ARGS
					-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-Wno-dev
					-DAPR_INSTALL_PRIVATE_H=ON
					-DINSTALL_PDB=OFF
			)
		else()
			set(APR_CONFIGS
				CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APR &&
					./configure --prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					--enable-shared=no
					--enable-static=yes
					--silent
				BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APR &&
					make "CFLAGS=${APACHE_CFLAGS}"
				INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APR &&
					make install
			)
		endif()
		ExternalProject_Add(APR
			${EP_CONFIGS}
			URL http://archive.apache.org/dist/apr/apr-1.5.2.tar.gz
			URL_MD5 98492e965963f852ab29f9e61b2ad700
			${APR_CONFIGS}
		)
		set_target_properties(APR PROPERTIES ${EP_PROPS})


		if(WIN32)
			set(APRutil_CONFIGS
				CMAKE_ARGS
					-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-Wno-dev
					-DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-DINSTALL_PDB=OFF
			)
		else()
			set(APRutil_CONFIGS
				CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
					./configure
						--prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
						--silent
						${APR_CONF}
						${OPENSSL_CONF}
				BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
					make "CFLAGS=${APACHE_CFLAGS}"
				INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
					make install
			)
		endif()
		ExternalProject_Add(APRutil
			${EP_CONFIGS}
			DEPENDS ${APR_DEP} ${OPENSSL_DEP}
			URL http://archive.apache.org/dist/apr/apr-util-1.5.4.tar.gz
			URL_MD5 866825c04da827c6e5f53daff5569f42
			${APRutil_CONFIGS}
		)
		set_target_properties(APRutil PROPERTIES ${EP_PROPS})


		if(WIN32)
			set(Apache_CONFIGS
				CMAKE_ARGS
					-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-Wno-dev
					-DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					"-DEXTRA_COMPILE_FLAGS=/D PCRE_STATIC"
					-DEXTRA_LIBS=Ws2_32
				BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/Apache-build &&
					${CMAKE_COMMAND} --build . --target libhttpd --config ${CMAKE_CFG_INTDIR}
				INSTALL_COMMAND echo skip install
			)
			set(APACHE_INCLUDE_DIRS
				${CMAKE_BINARY_DIR}/libs/src/install-prefix/include
				${CMAKE_BINARY_DIR}/libs/src/Apache/include
				${CMAKE_BINARY_DIR}/libs/src/Apache/os/win32
				${CMAKE_BINARY_DIR}/libs/src/Apache-build
			)
			set(APACHE_LIBRARIES
				${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libapr-1.lib
				${CMAKE_BINARY_DIR}/libs/src/Apache-build/${CMAKE_CFG_INTDIR}/libhttpd.lib
			)
		else()
			set(Apache_CONFIGS
				CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/Apache &&
					./configure
						--prefix=${CMAKE_BINARY_DIR}/libs/src/Apache-build
						--silent
						${APR_CONF}
						${APRUTIL_CONF}
						${OPENSSL_CONF}
						${PCRE_CONF}
						${ZLIB_CONF}
				BUILD_COMMAND echo skip build
				INSTALL_COMMAND echo skip install
			)
			set(APACHE_INCLUDE_DIRS
				${CMAKE_BINARY_DIR}/libs/src/install-prefix/include/apr-1
				${CMAKE_BINARY_DIR}/libs/src/Apache/include
				${CMAKE_BINARY_DIR}/libs/src/Apache/os/unix
			)
			set(APACHE_LIBRARIES

			)
		endif()
		ExternalProject_Add(Apache
			${EP_CONFIGS}
			DEPENDS ${APR_DEP} ${APRUTIL_DEP} ${OPENSSL_DEP} ${PCRE_DEP}
			URL http://archive.apache.org/dist/httpd/httpd-2.4.29.tar.gz
			URL_MD5 6380b0856658f07479fdcba9e20294a6
			${Apache_CONFIGS}
		)
		set_target_properties(Apache PROPERTIES ${EP_PROPS})
		# Download sources for fat source archive
		add_dependencies(download_static_deps Apache-download)
		add_dependencies(download_static_deps APR-download)
		add_dependencies(download_static_deps APRutil-download)
	else()
		find_package(APACHE REQUIRED)
		find_package(APR REQUIRED)
		set(APACHE_LIBRARIES ${APR_LIBRARIES} ${APRUTIL_LIBRARIES})
		if(HTTPD_LIBRARIES)
			list(APPEND APACHE_LIBRARIES ${HTTPD_LIBRARIES})
		endif()
		set(APACHE_INCLUDE_DIRS ${APACHE_INCLUDE_DIR} ${APR_INCLUDE_DIR} ${APRUTIL_INCLUDE_DIR})
	endif()

  add_subdirectory(mod_neko)
  add_subdirectory(mod_tora)
endif()
