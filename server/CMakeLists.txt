########################################################################
# Build the remote server application
########################################################################
add_executable(SoapySDRServer
    SoapyServer.cpp
    ServerListener.cpp
    ClientHandler.cpp
    LogForwarding.cpp
    ServerStreamData.cpp)

target_link_libraries(SoapySDRServer PRIVATE SoapySDR SoapySDRRemoteCommon)

target_include_directories(SoapySDRServer PRIVATE ${SOAPY_SERVER_SOURCES})
if (MSVC)
    target_include_directories(SoapySDRServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/msvc)
endif ()

install(TARGETS SoapySDRServer DESTINATION bin)

#install man pages for the application executable
install(FILES SoapySDRServer.1 DESTINATION share/man/man1)

########################################################################
# Thread config support
########################################################################
find_library(
    RT_LIBRARIES
    NAMES rt
    PATHS /usr/lib /usr/lib64
)

if (RT_LIBRARIES)
    target_link_libraries(SoapySDRServer PRIVATE ${RT_LIBRARIES})
endif()

if(WIN32)
    target_sources(SoapySDRServer PRIVATE ThreadPrioWindows.cpp)
elseif(UNIX)
    target_sources(SoapySDRServer PRIVATE ThreadPrioUnix.cpp)
endif()
