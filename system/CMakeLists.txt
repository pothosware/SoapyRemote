########################################################################
# SoapySDRServer systemd service
########################################################################
option(ENABLE_SYSTEMD "Install systemd unit" ON)
if(ENABLE_SYSTEMD)

    if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
        set(SYSTEMD_UNIT_INSTALL_DIR /usr/lib/systemd/system)
    else()
        #source installs will use the install prefix
        set(SYSTEMD_UNIT_INSTALL_DIR lib/systemd/system)
    endif()

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
option(ENABLE_SYSCTL "Install kernel tweaks for socket buffer sizes" ON)
if(ENABLE_SYSCTL)
    install(
        FILES SoapySDRServer.sysctl
        RENAME 10-SoapySDRServer.conf
        DESTINATION lib/sysctl.d)
endif()
