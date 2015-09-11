########################################################################
# Build the client plugin module
########################################################################
SOAPY_SDR_MODULE_UTIL(
    TARGET remoteSupport
    SOURCES
        SoapyClient.cpp
    LIBRARIES
        SoapySDRRemoteCommon
)
