########################################################################
# SoapySDRServer systemd service
########################################################################
option(ENABLE_SYSTEMD "Install systemd unit" ON)
if(ENABLE_SYSTEMD)

    set(SYSTEMD_UNIT_DIR "lib/systemd/system" CACHE STRING "Directory to install systemd unit")

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/SoapySDRServer.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/SoapySDRServer.service
    @ONLY)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SoapySDRServer.service
        DESTINATION "${SYSTEMD_UNIT_DIR}")
endif()

########################################################################
# Increase the sysctl network limits
########################################################################
option(ENABLE_SYSCTL "Install kernel tweaks for socket buffer sizes" ON)
if(ENABLE_SYSCTL)

    set(SYSCTL_CONFIG_DIR "lib/sysctl.d" CACHE STRING "Directory to install sysctl.d configs")

    install(
        FILES SoapySDRServer.sysctl
        RENAME 10-SoapySDRServer.conf
        DESTINATION "${SYSCTL_CONFIG_DIR}")
endif()
