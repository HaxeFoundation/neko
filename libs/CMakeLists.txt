add_subdirectory(common)
add_subdirectory(std)

if (STATIC_ZLIB)
	set(ZLIB_CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
		-Wno-dev
	)
	if (UNIX)
		list(APPEND ZLIB_CMAKE_ARGS
			-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
			-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
			${ARG_PIC}
		)
	endif()
	if (WIN32)
		set(ZLIB_LIBRARIES
			optimized ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/zlibstatic.lib
			debug ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/zlibstaticd.lib
		)
	else()
		set(ZLIB_LIBRARIES ${CMAKE_BINARY_DIR}/libs/src/install-prefix/lib/libz.a)
	endif()

	# Get the current config. Borrowed from
	# https://github.com/Kitware/CMake/blob/bc7d64f896d6e180970cb404cc7699732db34adc/Modules/ExternalProject.cmake
	if (CMAKE_CFG_INTDIR AND
		NOT CMAKE_CFG_INTDIR STREQUAL "." AND
		NOT CMAKE_CFG_INTDIR MATCHES "\\$")
		set(config ${CMAKE_CFG_INTDIR})
	else()
		set(config $<CONFIG>)
	endif()

	ExternalProject_Add(Zlib
		${EP_CONFIGS}
		URL
			https://zlib.net/fossils/zlib-1.2.13.tar.gz
			https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz
		URL_HASH SHA256=b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30
		CMAKE_ARGS ${ZLIB_CMAKE_ARGS}
		INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/Zlib-build &&
			${CMAKE_COMMAND} --build . --target install --config ${config}
		BYPRODUCTS ${ZLIB_LIBRARIES}
	)
	set_target_properties(Zlib PROPERTIES ${EP_PROPS})
	set(ZLIB_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libs/src/install-prefix/include)
	# Download project for fat source archive
	add_dependencies(download_deps Zlib-download)
else()
	find_package(ZLIB REQUIRED)
endif()

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
		if (STATIC_PCRE2)
			set(PCRE_CONF --with-pcre=${CMAKE_BINARY_DIR}/libs/src/install-prefix/bin/pcre2-config)
			set(PCRE_DEP pcre2)
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
		if (WIN32)
			set(EXPAT_CONF
				-DCMAKE_POLICY_DEFAULT_CMP0074=NEW
				-DEXPAT_ROOT=${CMAKE_BINARY_DIR}/libs/src/install-prefix
			)
			if (${CMAKE_VERSION} VERSION_LESS 3.12)
				message(WARNING "CMake 3.12 or above is required for building APRutil on Windows")
			endif()
			if (${CMAKE_VERSION} VERSION_LESS 3.27)
				# we need to make sure cmake finds the correct library build
				# see: https://gitlab.kitware.com/cmake/cmake/-/merge_requests/8225
				# this monstrosity is necessary because semicolons have to be escaped for each time the string variable is used
				string(REPLACE ";" "\\\\\\\\\\\\\;" EXPAT_LIBRARY_SUFFIXES_ESCAPED "${CMAKE_FIND_LIBRARY_SUFFIXES};dMD.lib;MD.lib;dMT.lib;MT.lib;d.lib")
				list(APPEND EXPAT_CONF "-DCMAKE_FIND_LIBRARY_SUFFIXES=${EXPAT_LIBRARY_SUFFIXES_ESCAPED}")
			endif()
			set(EXPAT_DEP EXPAT)
		elseif()
			set(EXPAT_CONF "")
			set(EXPAT_DEP "")
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
			URL https://archive.apache.org/dist/apr/apr-1.7.2.tar.gz
			URL_HASH SHA256=3d8999b216f7b6235343a4e3d456ce9379aa9a380ffb308512f133f0c5eb2db9
			${APR_CONFIGS}
		)
		set_target_properties(APR PROPERTIES ${EP_PROPS})

		if(WIN32)
			ExternalProject_Add(EXPAT
				${EP_CONFIGS}
				URL https://github.com/libexpat/libexpat/releases/download/R_2_5_0/expat-2.5.0.tar.gz
				URL_HASH SHA256=6b902ab103843592be5e99504f846ec109c1abb692e85347587f237a4ffa1033
				CMAKE_ARGS
					-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-Wno-dev
					-DEXPAT_SHARED_LIBS=OFF
			)
			set_target_properties(EXPAT PROPERTIES ${EP_PROPS})
			set(APRutil_CONFIGS
				CMAKE_ARGS
					-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-Wno-dev
					-DINSTALL_PDB=OFF
					${EXPAT_CONF}
				PATCH_COMMAND ${CMAKE_COMMAND} -Dapr-util_source=${CMAKE_BINARY_DIR}/libs/src/APRutil -P ${CMAKE_SOURCE_DIR}/cmake/patch_apr-util.cmake
			)
		else()
			set(APRutil_CONFIGS
				CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
					./configure
						--prefix=${CMAKE_BINARY_DIR}/libs/src/install-prefix
						--silent
						${APR_CONF}
				BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
					make "CFLAGS=${APACHE_CFLAGS}"
				INSTALL_COMMAND cd ${CMAKE_BINARY_DIR}/libs/src/APRutil &&
					make install
			)
		endif()

		ExternalProject_Add(APRutil
			${EP_CONFIGS}
			DEPENDS ${APR_DEP} ${EXPAT_DEP}
			URL https://archive.apache.org/dist/apr/apr-util-1.6.3.tar.gz
			URL_HASH SHA256=2b74d8932703826862ca305b094eef2983c27b39d5c9414442e9976a9acf1983
			${APRutil_CONFIGS}
		)
		set_target_properties(APRutil PROPERTIES ${EP_PROPS})

		if(WIN32)
			set(Apache_CONFIGS
				CMAKE_ARGS
					-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-Wno-dev
					-DOPENSSL_ROOT_DIR=${CMAKE_BINARY_DIR}/libs/src/install-prefix
					-DEXTRA_LIBS=Ws2_32
				PATCH_COMMAND PATCH_COMMAND ${CMAKE_COMMAND} -Dapache_source=${CMAKE_BINARY_DIR}/libs/src/Apache -P ${CMAKE_SOURCE_DIR}/cmake/patch_apache.cmake
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
						${OPENSSL_CONF}
						${PCRE_CONF}
						${ZLIB_CONF}
						${APRUTIL_CONF}
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
			DEPENDS ${APR_DEP} ${APRUTIL_DEP} ${OPENSSL_DEP} ${PCRE_DEP} ${ZLIB_DEP}
			URL
				https://archive.apache.org/dist/httpd/httpd-2.4.55.tar.gz
				https://github.com/HaxeFoundation/neko/files/10745746/httpd-2.4.55.tar.gz
			URL_HASH SHA256=5276ea8bc6fff31eed5c82132ae51a0b2ee05f9e6b61a00fa877f6cadab3b638
			${Apache_CONFIGS}
		)
		set_target_properties(Apache PROPERTIES ${EP_PROPS})
		# Download sources for fat source archive
		if (WIN32)
			add_dependencies(download_deps EXPAT-download)
		endif()
		add_dependencies(download_deps Apache-download)
		add_dependencies(download_deps APR-download)
		add_dependencies(download_deps APRutil-download)
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
