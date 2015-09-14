########################################################################
# Build the remote server application
########################################################################
set(SOAPY_SERVER_SOURCES
    SoapyServer.cpp
    ServerListener.cpp
    ClientHandler.cpp
    LogForwarding.cpp
)
if (MSVC)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/msvc)
endif ()
include_directories(${SoapySDR_INCLUDE_DIRS})
add_executable(SoapySDRServer ${SOAPY_SERVER_SOURCES})
target_link_libraries(SoapySDRServer ${SoapySDR_LIBRARIES} SoapySDRRemoteCommon)
install(TARGETS SoapySDRServer DESTINATION bin)
