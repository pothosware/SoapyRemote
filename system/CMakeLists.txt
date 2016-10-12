########################################################################
# SoapySDRServer systemd service
########################################################################
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapySDRServer.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/SoapySDRServer.service
@ONLY)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SoapySDRServer.service
    DESTINATION lib/systemd/system)

########################################################################
# Increase the sysctl network limits
########################################################################
install(
    FILES SoapySDRServer.sysctl
    RENAME SoapySDRServer.conf
    DESTINATION lib/sysctl.d)
