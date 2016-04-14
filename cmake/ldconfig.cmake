# Detect if the install is run by CPack.
if (NOT CMAKE_INSTALL_PREFIX MATCHES "/_CPack_Packages/.*/(TGZ|ZIP)/")
	message(STATUS "Running: ldconfig")
	execute_process(COMMAND "ldconfig" RESULT_VARIABLE ldconfig_result)
	if (NOT ldconfig_result EQUAL 0)
		message(WARNING "ldconfig failed")
	endif()
endif()