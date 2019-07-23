########################################################################
# SoapySDRServer systemd service
########################################################################

# Helper function to do The Right Thing with systemd.
function(detect_systemd_dir)
	find_package(PkgConfig)
	if(PKGCONFIG_FOUND)
		pkg_check_modules(SYSTEMD "systemd")
		if(SYSTEMD_FOUND)
			execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
				--variable=systemdsystemunitdir systemd
				OUTPUT_VARIABLE SYSTEMD_UNIT_INSTALL_DIR)
			string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_UNIT_INSTALL_DIR
				"${SYSTEMD_UNIT_INSTALL_DIR}")
		endif()
	endif()
	if(NOT DEFINED SYSTEMD_UNIT_INSTALL_DIR)
		# pkgconfig failed, set it to /lib/systemd/system and warn
		set(SYSTEMD_UNIT_INSTALL_DIR /lib/systemd/system)
	endif()
	# return the value
	set(SYSTEMD_UNIT_INSTALL_DIR ${SYSTEMD_UNIT_INSTALL_DIR} PARENT_SCOPE)
endfunction()

option(ENABLE_SYSTEMD "Install systemd unit" ${LINUX})
if(ENABLE_SYSTEMD)
	detect_systemd_dir()
	set(SYSTEMD_UNIT_DIR "${SYSTEMD_UNIT_INSTALL_DIR}" CACHE STRING "Directory to install systemd unit")
	configure_file(
	    ${CMAKE_CURRENT_SOURCE_DIR}/SoapySDRServer.service.in
	    ${CMAKE_CURRENT_BINARY_DIR}/SoapySDRServer.service
	@ONLY)
	install(FILES
	    ${CMAKE_CURRENT_BINARY_DIR}/SoapySDRServer.service
	    DESTINATION ${SYSTEMD_UNIT_DIR})
endif()

########################################################################
# Increase the sysctl network limits
########################################################################

option(ENABLE_SYSCTL  "Install kernel tweaks for socket buffer sizes" OFF)
if(ENABLE_SYSCTL)
	install(
	    FILES SoapySDRServer.sysctl
	    RENAME 10-SoapySDRServer.conf
	    DESTINATION lib/sysctl.d)
endif()
