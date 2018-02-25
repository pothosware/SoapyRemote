########################################################################
# Build the client plugin module
########################################################################
SOAPY_SDR_MODULE_UTIL(
    TARGET remoteSupport
    SOURCES
        Registration.cpp
        Settings.cpp
        Streaming.cpp
        LogAcceptor.cpp
        ClientStreamData.cpp
        DiscoverServers.cpp
    LIBRARIES
        SoapySDRRemoteCommon
)
