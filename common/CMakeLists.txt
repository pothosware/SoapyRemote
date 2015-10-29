########################################################################
# Build the remote support library
########################################################################
set(COMMON_SOURCES
    SoapyURLUtils.cpp
    SoapyRPCSocket.cpp
    SoapyRPCPacker.cpp
    SoapyRPCUnpacker.cpp
    SoapyStreamEndpoint.cpp
    SoapyHTTPUtils.cpp
    SoapySSDPEndpoint.cpp
)

#configure ssdp defines
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapyInfoUtils.in.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapyInfoUtils.cpp
@ONLY)
list(APPEND COMMON_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/SoapyInfoUtils.cpp)

#check for platform-specific network header
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(winsock2.h HAS_WINSOCK2_H)
CHECK_INCLUDE_FILES(ws2tcpip.h HAS_WS2TCPIP_H)
CHECK_INCLUDE_FILES(netdb.h HAS_NETDB_H)
CHECK_INCLUDE_FILES(unistd.h HAS_UNISTD_H)
CHECK_INCLUDE_FILES(netinet/in.h HAS_NETINET_IN_H)
CHECK_INCLUDE_FILES(netinet/tcp.h HAS_NETINET_TCP_H)
CHECK_INCLUDE_FILES(sys/types.h HAS_SYS_TYPES_H)
CHECK_INCLUDE_FILES(sys/socket.h HAS_SYS_SOCKET_H)
CHECK_INCLUDE_FILES(arpa/inet.h HAS_ARPA_INET_H)
CHECK_INCLUDE_FILES(ifaddrs.h HAS_IFADDRS_H)
CHECK_INCLUDE_FILES(net/if.h HAS_NET_IF_H)

#network libraries
if (WIN32)
    list(APPEND SoapySDR_LIBRARIES ws2_32)
endif (WIN32)

#create private include header for network compatibility
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapySocketDefs.in.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapySocketDefs.hpp)

#build a static library
include_directories(${SoapySDR_INCLUDE_DIRS})
add_library(SoapySDRRemoteCommon STATIC ${COMMON_SOURCES})
target_link_libraries(SoapySDRRemoteCommon ${SoapySDR_LIBRARIES})
set_property(TARGET SoapySDRRemoteCommon PROPERTY POSITION_INDEPENDENT_CODE TRUE)
